#include "VRM.hpp"

#include <threepp/loaders/GLTFLoader.hpp>

// VRM.h は nlohmann::json を前提にするので先に include する。
#include <nlohmann/json.hpp>

#ifndef USE_VRMC_VRM_1_0
#define USE_VRMC_VRM_1_0// 通常は CMake の DEFINITIONS で定義済み
#endif
#include <VRMC/VRM.h>

#include <threepp/math/Euler.hpp>
#include <threepp/math/Matrix4.hpp>
#include <threepp/materials/MeshToonMaterial.hpp>
#include <threepp/materials/MeshStandardMaterial.hpp>
#include <threepp/materials/ShaderMaterial.hpp>
#include <threepp/textures/DataTexture.hpp>
#include <threepp/objects/SkinnedMesh.hpp>
#include <threepp/core/Uniform.hpp>
#include <threepp/math/Vector2.hpp>
#include <threepp/math/Vector3.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <functional>
#include <iostream>
#include <unordered_map>

namespace krkrvrm {

    namespace {
        std::shared_ptr<threepp::Texture> whiteDummyTexture();// 前方宣言

        // MToon outline (反転ハル法)。法線方向に outlineWidth 押し出した back-face のみを
        // 描いてモデル背後にはみ出させ輪郭線にする。スキニング対応(SkinnedMesh でも変形追従)。
        std::shared_ptr<threepp::ShaderMaterial> makeOutlineMaterial(const threepp::Color& color, float width,
                                                                    threepp::Texture* baseMap = nullptr,
                                                                    float alphaCutoff = 0.f) {
            auto mat = threepp::ShaderMaterial::create();
            // three-vrm 準拠: オブジェクト空間で objectNormal に沿ってオフセットし、
            // z を僅かに奥へ押して z-fight を防ぐ。view 空間 push は鋭いエッジで
            // 反転ハルが裏返り大きなフラップとして飛び出すため使わない。
            mat->vertexShader = R"(
#include <common>
#include <morphtarget_pars_vertex>
#include <skinning_pars_vertex>
uniform float outlineWidth;
varying vec2 vUvO;
void main() {
    vUvO = uv;
    #include <beginnormal_vertex>
    #include <morphnormal_vertex>
    #include <skinbase_vertex>
    #include <skinnormal_vertex>
    #include <begin_vertex>
    #include <morphtarget_vertex>
    #include <skinning_vertex>
    vec3 tNormal = normalMatrix * objectNormal;
    vec3 outlineOffset = outlineWidth * length( tNormal ) * objectNormal;
    gl_Position = projectionMatrix * modelViewMatrix * vec4( outlineOffset + transformed, 1.0 );
    gl_Position.z += 1e-6 * gl_Position.w;
}
)";
            // surface と同じアルファ discard を行う。これをしないとフリル(レース)等の
            // 透明部分まで outline が単色で塗られ、透明三角が△として飛び出す。
            mat->fragmentShader = R"(
uniform vec3 outlineColor;
uniform sampler2D baseMap;
uniform int hasBaseMap;
uniform float alphaCutoff;
varying vec2 vUvO;
void main() {
    if ( hasBaseMap != 0 && alphaCutoff > 0.0 ) {
        if ( texture2D( baseMap, vUvO ).a < alphaCutoff ) discard;
    }
    gl_FragColor = vec4( outlineColor, 1.0 );
}
)";
            mat->uniforms = threepp::UniformMap{
                    {"outlineWidth", threepp::Uniform(width)},
                    {"outlineColor", threepp::Uniform(threepp::Vector3(color.r, color.g, color.b))},
                    {"baseMap", threepp::Uniform(baseMap ? baseMap : whiteDummyTexture().get())},
                    {"hasBaseMap", threepp::Uniform(baseMap ? 1 : 0)},
                    {"alphaCutoff", threepp::Uniform(alphaCutoff)}};
            mat->side = threepp::Side::Back;// 反転ハル(front cull)
            mat->lights = false;
            return mat;
        }


        // MToon 本体 (lights 有効の ShaderMaterial)。lit/shade トゥーン陰影を
        // shadingShift/shadingToony のランプで表現し、rim(フレネル)/emissive を統合する。
        // shade は baseColorTexture を共有し shadeColorFactor で tint する(VRM の一般ケース)。
        // skinning は SkinnedMesh で自動有効、morph は morphTargets フラグ(呼び出し側で設定)。
        struct MToonShaderParams {
            threepp::Color litColor{1.f, 1.f, 1.f};
            threepp::Color shadeColor{1.f, 1.f, 1.f};
            float shadingShift = -0.05f;
            float shadingToony = 0.9f;
            threepp::Texture* baseMap = nullptr;
            threepp::Color emissive{0.f, 0.f, 0.f};
            threepp::Texture* emissiveMap = nullptr;
            threepp::Color rimColor{0.f, 0.f, 0.f};
            float rimPower = 5.f, rimLift = 0.f;
            threepp::Texture* normalMap = nullptr;
            float normalScale = 1.f;
            threepp::Texture* matcapMap = nullptr;      ///< 0.x _SphereAdd / 1.0 matcapTexture (加算)
            threepp::Color matcapColor{1.f, 1.f, 1.f};
            float opacity = 1.f, alphaTest = 0.f;
            bool transparent = false;
            threepp::Side side = threepp::Side::Front;
        };

        // sampler uniform に null Texture* を渡すと threepp がバインド時にクラッシュするため、
        // テクスチャの無いスロット用に 1x1 白ダミーを共有で用意する。
        std::shared_ptr<threepp::Texture> whiteDummyTexture() {
            static std::shared_ptr<threepp::Texture> tex;
            if (!tex) {
                std::vector<unsigned char> px{255, 255, 255, 255};
                auto t = threepp::DataTexture::create<unsigned char>(4, 1, 1);
                t->image().setData(threepp::ImageData(std::move(px)));
                t->needsUpdate();
                tex = t;
            }
            return tex;
        }

        std::shared_ptr<threepp::ShaderMaterial> makeMToonMaterial(const MToonShaderParams& p) {
            auto mat = threepp::ShaderMaterial::create();
            mat->vertexShader = R"(
#include <common>
#include <morphtarget_pars_vertex>
#include <skinning_pars_vertex>
varying vec2 vMUv;
varying vec3 vMNormal;
varying vec3 vMViewPos;
void main() {
    vMUv = uv;
    #include <beginnormal_vertex>
    #include <morphnormal_vertex>
    #include <skinbase_vertex>
    #include <skinnormal_vertex>
    #include <begin_vertex>
    #include <morphtarget_vertex>
    #include <skinning_vertex>
    vec4 mvPosition = modelViewMatrix * vec4( transformed, 1.0 );
    vMViewPos = -mvPosition.xyz;
    vMNormal  = normalize( normalMatrix * objectNormal );
    gl_Position = projectionMatrix * mvPosition;
}
)";
            mat->fragmentShader = R"(
uniform vec3 litFactor;
uniform vec3 shadeFactor;
uniform float shadingShift;
uniform float shadingToony;
uniform float opacity;
uniform float alphaCutoff;
uniform int opaqueMode;        // 1=不透明(αを1に固定)。テクスチャαを無視する
uniform vec3 emissiveFactor;
uniform vec3 rimFactor;
uniform float rimPower;
uniform float rimLift;
uniform vec3 keyLightDir;    // view 空間の主光源方向 (面→光源)
uniform vec3 keyLightColor;
uniform vec3 ambientColor;
uniform vec3 matcapColor;
uniform float normalScale;
uniform vec2 uvOffset;         // textureTransformBind / UVアニメ scroll: uv = rot(vMUv)*uvScale + uvOffset
uniform vec2 uvScale;
uniform float uvRotPhase;      // UVアニメ回転 (中心まわり, ラジアン)
uniform sampler2D baseMap;     uniform int hasBaseMap;
uniform sampler2D emissiveMap; uniform int hasEmissiveMap;
uniform sampler2D normalMap;   uniform int hasNormalMap;
uniform sampler2D matcapMap;   uniform int hasMatcap;
varying vec2 vMUv;
varying vec3 vMNormal;
varying vec3 vMViewPos;
float linstep( float a, float b, float t ) { return clamp( ( t - a ) / max( 1e-4, b - a ), 0.0, 1.0 ); }
// tangent 属性が無い VRM 向け: 画面微分から接空間を組んで法線マップを適用 (three.js perturbNormal2Arb 相当)
vec3 perturbNormal( vec3 N, vec3 pos, vec2 uv, vec3 mapN ) {
    vec3 q0 = dFdx( pos ); vec3 q1 = dFdy( pos );
    vec2 st0 = dFdx( uv ); vec2 st1 = dFdy( uv );
    vec3 S = normalize( q0 * st1.t - q1 * st0.t );
    vec3 T = normalize( -q0 * st1.s + q1 * st0.s );
    return normalize( mat3( S, T, N ) * mapN );
}
void main() {
    vec2 uv = vMUv;
    if ( uvRotPhase != 0.0 ) {              // UVアニメ回転 (中心まわり)
        float c = cos( uvRotPhase ), s = sin( uvRotPhase );
        uv = mat2( c, -s, s, c ) * ( uv - 0.5 ) + 0.5;
    }
    uv = uv * uvScale + uvOffset;          // textureTransformBind / UVアニメ scroll
    vec4 tex = ( hasBaseMap != 0 ) ? texture2D( baseMap, uv ) : vec4( 1.0 );
    vec3 lit   = litFactor   * tex.rgb;
    vec3 shade = shadeFactor * tex.rgb;
    float alpha = opacity * tex.a;
    if ( alphaCutoff > 0.0 && alpha < alphaCutoff ) discard;// cutout
    if ( opaqueMode != 0 ) alpha = 1.0;                    // 不透明材はαを無視(背景が透けるのを防ぐ)
    vec3 pos = -vMViewPos;                 // view 空間の面位置 (= mvPosition.xyz)
    vec3 N = normalize( vMNormal );
    if ( hasNormalMap != 0 ) {
        vec3 mapN = texture2D( normalMap, uv ).xyz * 2.0 - 1.0;
        mapN.xy *= normalScale;
        N = perturbNormal( N, pos, uv, mapN );
    }
    vec3 V = normalize( vMViewPos );
    float dotNL = dot( N, normalize( keyLightDir ) );
    float sh = linstep( -1.0 + shadingToony, 1.0 - shadingToony, dotNL + shadingShift );
    vec3 col = mix( shade, lit, sh ) * keyLightColor;
    col += lit * ambientColor;                       // ambient/GI は lit 全開扱い
    col += emissiveFactor * ( ( hasEmissiveMap != 0 ) ? texture2D( emissiveMap, uv ).rgb : vec3( 1.0 ) );
    float f = clamp( 1.0 - max( 0.0, dot( N, V ) ) + rimLift, 0.0, 1.0 );
    col += rimFactor * pow( f, max( 0.001, rimPower ) );
    if ( hasMatcap != 0 ) {                // matcap (0.x _SphereAdd 相当): three-vrm 準拠の球UVで加算
        vec3 mx = normalize( vec3( V.z, 0.0, -V.x ) );
        vec3 my = cross( V, mx );
        vec2 muv = 0.5 + 0.5 * vec2( dot( mx, N ), -dot( my, N ) );
        col += matcapColor * texture2D( matcapMap, muv ).rgb;
    }
    gl_FragColor = vec4( col, alpha );
}
)";
            auto v3 = [](const threepp::Color& c) { return threepp::Vector3(c.r, c.g, c.b); };
            mat->uniforms = threepp::UniformMap{
                    {"litFactor", threepp::Uniform(v3(p.litColor))},
                    {"shadeFactor", threepp::Uniform(v3(p.shadeColor))},
                    {"shadingShift", threepp::Uniform(p.shadingShift)},
                    {"shadingToony", threepp::Uniform(p.shadingToony)},
                    {"opacity", threepp::Uniform(p.opacity)},
                    {"alphaCutoff", threepp::Uniform(p.alphaTest)},
                    {"opaqueMode", threepp::Uniform(p.transparent ? 0 : 1)},
                    {"emissiveFactor", threepp::Uniform(v3(p.emissive))},
                    {"rimFactor", threepp::Uniform(v3(p.rimColor))},
                    {"rimPower", threepp::Uniform(p.rimPower)},
                    {"rimLift", threepp::Uniform(p.rimLift)},
                    {"matcapColor", threepp::Uniform(v3(p.matcapColor))},
                    {"normalScale", threepp::Uniform(p.normalScale)},
                    {"uvOffset", threepp::Uniform(threepp::Vector2(0.f, 0.f))},
                    {"uvScale", threepp::Uniform(threepp::Vector2(1.f, 1.f))},
                    {"uvRotPhase", threepp::Uniform(0.f)},
                    {"baseMap", threepp::Uniform(p.baseMap ? p.baseMap : whiteDummyTexture().get())},
                    {"hasBaseMap", threepp::Uniform(p.baseMap ? 1 : 0)},
                    {"emissiveMap", threepp::Uniform(p.emissiveMap ? p.emissiveMap : whiteDummyTexture().get())},
                    {"hasEmissiveMap", threepp::Uniform(p.emissiveMap ? 1 : 0)},
                    {"normalMap", threepp::Uniform(p.normalMap ? p.normalMap : whiteDummyTexture().get())},
                    {"hasNormalMap", threepp::Uniform(p.normalMap ? 1 : 0)},
                    {"matcapMap", threepp::Uniform(p.matcapMap ? p.matcapMap : whiteDummyTexture().get())},
                    {"hasMatcap", threepp::Uniform(p.matcapMap ? 1 : 0)},
                    // 固定キーライト(view 空間・カメラ相対)。threepp lights=true の ShaderMaterial は
                    // renderer 側 uniform 前提で不安定なため、自前の単一ライトで MToon 陰影を出す。
                    {"keyLightDir", threepp::Uniform(threepp::Vector3(0.25f, 0.40f, 1.0f))},
                    {"keyLightColor", threepp::Uniform(threepp::Vector3(0.7f, 0.7f, 0.7f))},
                    {"ambientColor", threepp::Uniform(threepp::Vector3(0.3f, 0.3f, 0.32f))}};
            mat->lights = false;
            mat->transparent = p.transparent;
            mat->opacity = p.opacity;
            mat->side = p.side;
            return mat;
        }

        // three-vrm 方式の outline: 別メッシュではなく、同一メッシュに 2 つ目のマテリアル
        // (outline, BackSide) を持たせ、geometry group を 2 つ張って同一頂点を2回描く。
        // 別 child SkinnedMesh 方式は deform 時にスキンがわずかにずれ反転ハルが flap として
        // 飛び出すため使わない (実機で確認)。
        void addMToonOutline(const std::shared_ptr<threepp::Mesh>& src,
                             const std::shared_ptr<threepp::Material>& outlineMat) {
            auto geo = src->geometry();
            if (!geo || !geo->groups.empty()) return;// 既存 group あり(多プリミティブ)は非対応
            auto surface = src->material();
            if (!surface) return;
            src->setMaterials({surface, outlineMat});
            const int count = geo->hasIndex() ? geo->getIndex()->count()
                                              : geo->getAttribute("position")->count();
            geo->addGroup(0, count, 0);// surface
            geo->addGroup(0, count, 1);// outline (同一頂点)
        }
    }// namespace

    // -----------------------------------------------------------------------
    // VRMHumanoid
    // -----------------------------------------------------------------------
    std::shared_ptr<threepp::Bone> VRMHumanoid::getRawBoneNode(const std::string& humanBoneName) const {
        auto it = rawBones_.find(humanBoneName);
        return it != rawBones_.end() ? it->second : nullptr;
    }

    bool VRMHumanoid::has(const std::string& humanBoneName) const {
        return rawBones_.find(humanBoneName) != rawBones_.end();
    }

    std::vector<std::string> VRMHumanoid::boneNames() const {
        std::vector<std::string> names;
        names.reserve(rawBones_.size());
        for (const auto& [name, _] : rawBones_) names.push_back(name);
        return names;
    }

    // human bone の親候補 (近い順)。最初に存在するものを親とする。
    // 指ボーンは rig に含めない (Phase1。raw rest のまま)。
    static std::vector<std::string> parentCandidates(const std::string& b) {
        auto side = [&](const char* L, const char* R) -> std::string {
            if (b.rfind("left", 0) == 0) return std::string("left") + (L + 4);
            if (b.rfind("right", 0) == 0) return std::string("right") + (R + 5);
            return "";
        };
        (void)side;
        if (b == "hips") return {};
        if (b == "spine") return {"hips"};
        if (b == "chest") return {"spine", "hips"};
        if (b == "upperChest") return {"chest", "spine"};
        if (b == "neck") return {"upperChest", "chest", "spine"};
        if (b == "head") return {"neck", "upperChest", "chest"};
        if (b == "leftEye" || b == "rightEye" || b == "jaw") return {"head", "neck"};

        auto pre = [&](const std::string& s) { return b.rfind(s, 0) == 0; };
        std::string lr = pre("left") ? "left" : (pre("right") ? "right" : "");
        auto suf = [&](const std::string& s) { return lr.empty() ? std::string() : lr + s; };

        if (b == suf("Shoulder")) return {"upperChest", "chest", "spine"};
        if (b == suf("UpperArm")) return {suf("Shoulder"), "upperChest", "chest"};
        if (b == suf("LowerArm")) return {suf("UpperArm")};
        if (b == suf("Hand")) return {suf("LowerArm")};

        if (b == suf("UpperLeg")) return {"hips"};
        if (b == suf("LowerLeg")) return {suf("UpperLeg")};
        if (b == suf("Foot")) return {suf("LowerLeg")};
        if (b == suf("Toes")) return {suf("Foot")};

        return {};// 未対応(指等)は rig に含めない
    }

    void VRMHumanoid::buildRig() {
        rig_.clear();
        rigOrder_.clear();
        rigRoot_ = threepp::Group::create();
        rigRoot_->name = "vrm_normalized_root";

        // 対象ボーン(親候補が定義され、親が存在 or ルート)を選ぶ
        auto resolveParent = [&](const std::string& name) -> std::string {
            for (const auto& c : parentCandidates(name))
                if (rawBones_.count(c)) return c;
            return "";// hips のみ空を正規のルートとする
        };

        for (const auto& [name, raw] : rawBones_) {
            // rig 対象判定: hips か、親候補が解決できるもの
            if (name != "hips") {
                const auto cands = parentCandidates(name);
                if (cands.empty()) continue;// 指等は除外
                bool ok = false;
                for (const auto& c : cands)
                    if (rawBones_.count(c)) { ok = true; break; }
                if (!ok) continue;
            }
            RigBone rb;
            rb.raw = raw;
            threepp::Quaternion wq;
            raw->getWorldQuaternion(wq);
            rb.rawRestWorldRot.copy(wq);
            rb.parent = (name == "hips") ? "" : resolveParent(name);
            rb.norm = threepp::Bone::create();
            rb.norm->name = "norm_" + name;
            rig_[name] = rb;
        }

        // 階層構築 + 親→子順の確定 (parent が置かれたものから順に)
        std::size_t placed = 0;
        std::vector<char> done;
        while (placed < rig_.size()) {
            std::size_t before = placed;
            for (auto& [name, rb] : rig_) {
                if (std::find(rigOrder_.begin(), rigOrder_.end(), name) != rigOrder_.end()) continue;
                const bool rootBone = rb.parent.empty();
                const bool parentPlaced = !rootBone && rig_.count(rb.parent) &&
                        std::find(rigOrder_.begin(), rigOrder_.end(), rb.parent) != rigOrder_.end();
                if (rootBone || parentPlaced) {
                    auto parentNode = rootBone ? std::static_pointer_cast<threepp::Object3D>(rigRoot_)
                                               : std::static_pointer_cast<threepp::Object3D>(rig_[rb.parent].norm);
                    // 正規化ノードの local 位置 = raw world 位置の親からの差分 (translation 保持)
                    threepp::Vector3 wp;
                    rb.raw->getWorldPosition(wp);
                    threepp::Vector3 pwp;
                    if (rootBone) pwp.set(0.f, 0.f, 0.f);
                    else rig_[rb.parent].raw->getWorldPosition(pwp);
                    rb.norm->position.copy(wp).sub(pwp);
                    if (name == "hips") {
                        hasHips_ = true;
                        hipsRestNormPos_.copy(rb.norm->position);
                        hipsRestWorldY_ = wp.y;
                    }
                    parentNode->add(rb.norm);
                    rigOrder_.push_back(name);
                    ++placed;
                }
            }
            if (placed == before) break;// 循環等の保険
        }
    }

    std::shared_ptr<threepp::Object3D> VRMHumanoid::getNormalizedBoneNode(const std::string& name) const {
        auto it = rig_.find(name);
        return it != rig_.end() ? it->second.norm : nullptr;
    }

    void VRMHumanoid::resetNormalizedPose() {
        for (auto& [name, rb] : rig_)
            rb.norm->quaternion.set(0.f, 0.f, 0.f, 1.f);
    }

    void VRMHumanoid::updateRig() {
        if (!rigRoot_) return;
        rigRoot_->updateMatrixWorld(true);
        for (const auto& name : rigOrder_) {
            auto& rb = rig_[name];
            threepp::Quaternion nw;
            rb.norm->getWorldQuaternion(nw);// 正規化 world 回転 (= rest からの world デルタ)
            threepp::Quaternion target;
            target.copy(nw).multiply(rb.rawRestWorldRot);// raw rest world に world デルタを乗算

            threepp::Quaternion pw;
            if (rb.raw->parent) {
                rb.raw->parent->updateWorldMatrix(true, false);
                rb.raw->parent->getWorldQuaternion(pw);
            }
            threepp::Quaternion local;
            local.copy(pw).invert().multiply(target);// parentWorld^-1 * target
            rb.raw->quaternion.copy(local);

            // hips(ルート)は位置も転写: 正規化ノードの world 位置を raw hips の親ローカルへ。
            // (rest では正規化 hips=raw hips world 位置なので no-op。ルートモーション時のみ移動)
            if (name == "hips") {
                threepp::Vector3 nwp;
                rb.norm->getWorldPosition(nwp);// 正規化 hips の world 位置
                if (rb.raw->parent) {
                    rb.raw->parent->updateWorldMatrix(true, false);
                    threepp::Matrix4 inv;
                    inv.copy(*rb.raw->parent->matrixWorld).invert();
                    nwp.applyMatrix4(inv);
                }
                rb.raw->position.copy(nwp);
            }
        }
    }

    // -----------------------------------------------------------------------
    // VRMExpressionManager
    // -----------------------------------------------------------------------
    void VRMExpressionManager::setValue(const std::string& name, float weight) {
        values_[name] = weight;
    }

    float VRMExpressionManager::getValue(const std::string& name) const {
        auto it = values_.find(name);
        return it != values_.end() ? it->second : 0.f;
    }

    bool VRMExpressionManager::has(const std::string& name) const {
        return expressions_.find(name) != expressions_.end();
    }

    std::vector<std::string> VRMExpressionManager::names() const {
        std::vector<std::string> v;
        v.reserve(expressions_.size());
        for (const auto& [n, _] : expressions_) v.push_back(n);
        return v;
    }

    namespace {
        // 表情名 -> グループ (blink/mouth/lookAt)。override はこのグループ単位で効く。
        bool isBlinkName(const std::string& n) {
            return n == "blink" || n == "blinkLeft" || n == "blinkRight";
        }
        bool isMouthName(const std::string& n) {
            return n == "aa" || n == "ee" || n == "ih" || n == "oh" || n == "ou";
        }
        bool isLookAtName(const std::string& n) {
            return n == "lookUp" || n == "lookDown" || n == "lookLeft" || n == "lookRight";
        }
    }// namespace

    void VRMExpressionManager::applyToScene() {
        // 各表情の実効ウェイト (binary 適用済み) を先に確定
        auto effWeight = [&](const std::string& name, const Expression& e) -> float {
            auto vit = values_.find(name);
            float v = (vit != values_.end()) ? vit->second : 0.f;
            if (v < 0.f) v = 0.f;
            if (v > 1.f) v = 1.f;
            if (e.isBinary) v = (v >= 0.5f) ? 1.f : 0.f;
            return v;
        };

        // 1) override 集計: block=活性なら 1 減算, blend=weight 減算。max(0,1-Σ) が抑制係数。
        auto amount = [](Override o, float w) -> float {
            switch (o) {
                case Override::Block: return w > 0.f ? 1.f : 0.f;
                case Override::Blend: return w;
                default: return 0.f;
            }
        };
        float mBlink = 1.f, mMouth = 1.f, mLookAt = 1.f;
        for (auto& [name, expr] : expressions_) {
            const float w = effWeight(name, expr);
            if (w <= 0.f) continue;
            mBlink -= amount(expr.overrideBlink, w);
            mMouth -= amount(expr.overrideMouth, w);
            mLookAt -= amount(expr.overrideLookAt, w);
        }
        mBlink = std::max(0.f, mBlink);
        mMouth = std::max(0.f, mMouth);
        mLookAt = std::max(0.f, mLookAt);
        lookAtMul_ = mLookAt;

        // 2) bind 対象の morph 影響度を 0 クリア
        for (auto& [name, expr] : expressions_) {
            for (auto& b : expr.morphBinds) {
                if (!b.mesh) continue;
                auto& inf = b.mesh->morphTargetInfluences();
                if (b.index >= 0 && static_cast<std::size_t>(b.index) < inf.size())
                    inf[b.index] = 0.f;
            }
        }
        // 3) 各表情ウェイト(グループ抑制込み) x bind ウェイトを加算
        for (auto& [name, expr] : expressions_) {
            float v = effWeight(name, expr);
            if (v <= 0.f) continue;
            if (isBlinkName(name)) v *= mBlink;
            else if (isMouthName(name)) v *= mMouth;
            else if (isLookAtName(name)) v *= mLookAt;
            if (v <= 0.f) continue;
            for (auto& b : expr.morphBinds) {
                if (!b.mesh) continue;
                auto& inf = b.mesh->morphTargetInfluences();
                if (b.index >= 0 && static_cast<std::size_t>(b.index) < inf.size())
                    inf[b.index] += v * b.weight;
            }
        }
        // 4) textureTransformBind: 表情ウェイトで材の UV offset/scale を動かす(瞳移動等)。
        //    材ごとに Σ weight*delta を集計し uvOffset/uvScale uniform を更新する。
        if (hasTexBinds_) {
            struct Acc { std::shared_ptr<threepp::ShaderMaterial> mat; float ox = 0, oy = 0, sx = 0, sy = 0; };
            std::unordered_map<threepp::ShaderMaterial*, Acc> acc;
            for (auto& [name, expr] : expressions_)
                for (auto& tb : expr.texBinds)
                    if (tb.material) acc[tb.material.get()].mat = tb.material;// 0 で登録(リセット)
            for (auto& [name, expr] : expressions_) {
                if (expr.texBinds.empty()) continue;
                float v = effWeight(name, expr);
                if (isBlinkName(name)) v *= mBlink;
                else if (isMouthName(name)) v *= mMouth;
                else if (isLookAtName(name)) v *= mLookAt;
                if (v <= 0.f) continue;
                for (auto& tb : expr.texBinds) {
                    if (!tb.material) continue;
                    auto& a = acc[tb.material.get()];
                    a.ox += tb.deltaOffset.x * v; a.oy += tb.deltaOffset.y * v;
                    a.sx += tb.deltaScale.x * v;  a.sy += tb.deltaScale.y * v;
                }
            }
            for (auto& [ptr, a] : acc) {
                a.mat->uniforms.at("uvOffset").setValue(threepp::Vector2(a.ox, a.oy));
                a.mat->uniforms.at("uvScale").setValue(threepp::Vector2(1.f + a.sx, 1.f + a.sy));
                a.mat->uniformsNeedUpdate = true;
            }
        }
    }

    void VRMExpressionManager::resolveTexBinds(
            const std::unordered_map<int, std::shared_ptr<threepp::ShaderMaterial>>& matByIdx) {
        hasTexBinds_ = false;
        for (auto& [name, expr] : expressions_) {
            expr.texBinds.clear();
            for (const auto& raw : expr.texBindsRaw) {
                auto it = matByIdx.find(raw.matIndex);
                if (it == matByIdx.end() || !it->second) continue;
                TexBind tb;
                tb.material = it->second;
                // initial は uvOffset=(0,0)/uvScale=(1,1)。delta = target - initial。
                tb.deltaOffset.set(raw.offset.x, raw.offset.y);
                tb.deltaScale.set(raw.scale.x - 1.f, raw.scale.y - 1.f);
                expr.texBinds.push_back(std::move(tb));
                hasTexBinds_ = true;
            }
        }
    }

    // -----------------------------------------------------------------------
    // VRMLookAt (bone 方式)
    // -----------------------------------------------------------------------
    void VRMLookAt::setTarget(float x, float y, float z) {
        target_.set(x, y, z);
        hasTarget_ = true;
    }

    void VRMLookAt::clearTarget() {
        hasTarget_ = false;
        if (leftEye_) leftEye_->quaternion.copy(leftRest_);
        if (rightEye_) rightEye_->quaternion.copy(rightRest_);
    }

    void VRMLookAt::update() {
        if (!valid() || !hasTarget_) return;

        head_->updateWorldMatrix(true, false);
        threepp::Vector3 headPos;
        head_->getWorldPosition(headPos);
        threepp::Quaternion headQuat;
        head_->getWorldQuaternion(headQuat);

        threepp::Vector3 off;
        off.copy(offset_).applyQuaternion(headQuat);
        threepp::Vector3 eyePos;
        eyePos.copy(headPos).add(off);

        // 注視方向を頭ローカル空間へ (VRM は +Z 正面)
        threepp::Vector3 dir;
        dir.subVectors(target_, eyePos);
        threepp::Quaternion inv;
        inv.copy(headQuat).invert();
        dir.applyQuaternion(inv).normalize();

        constexpr float RAD2DEG = 57.29577951308232f;
        constexpr float DEG2RAD = 0.017453292519943295f;
        const float yawDeg = std::atan2(dir.x, dir.z) * RAD2DEG;
        const float pitchDeg = std::asin(std::max(-1.f, std::min(1.f, dir.y))) * RAD2DEG;

        // rangeMap: clamp(t,0,inputMax)/inputMax * outputScale
        auto mapv = [](float t, float inMax, float outScale) {
            const float m = inMax > 1e-4f ? inMax : 1.f;
            return std::min(std::max(t, 0.f), inMax) / m * outScale;
        };

        // expression 方式: lookLeft/Right/Up/Down 表情ウェイトを駆動 (three-vrm
        // VRMLookAtExpressionApplier 準拠)。applyToScene がこの後に走り、表情に付いた
        // textureTransformBind で瞳の UV が動く。eye ボーンは回さない。
        if (expressionMode_ && exprMgr_) {
            if (pitchDeg < 0.f) {// 上を見る
                exprMgr_->setValue("lookDown", 0.f);
                exprMgr_->setValue("lookUp", mapv(-pitchDeg, inMaxV_, outVUp_));
            } else {
                exprMgr_->setValue("lookUp", 0.f);
                exprMgr_->setValue("lookDown", mapv(pitchDeg, inMaxV_, outVDown_));
            }
            if (yawDeg < 0.f) {
                exprMgr_->setValue("lookLeft", 0.f);
                exprMgr_->setValue("lookRight", mapv(-yawDeg, inMaxH_, outH_));
            } else {
                exprMgr_->setValue("lookRight", 0.f);
                exprMgr_->setValue("lookLeft", mapv(yawDeg, inMaxH_, outH_));
            }
            return;
        }

        // bone 方式: rangeMap (線形近似): |角度| を inputMax でクランプ/正規化し outputScale 倍
        const float ay = std::min(std::abs(yawDeg), inMaxH_) / inMaxH_ * outH_;
        const float yawOut = (yawDeg < 0.f) ? -ay : ay;
        const float vScale = (pitchDeg >= 0.f) ? outVUp_ : outVDown_;
        const float ap = std::min(std::abs(pitchDeg), inMaxV_) / inMaxV_ * vScale;
        const float pitchOut = (pitchDeg < 0.f) ? -ap : ap;

        threepp::Quaternion delta;
        delta.setFromEuler(threepp::Euler(pitchOut * DEG2RAD, yawOut * DEG2RAD, 0.f));
        if (leftEye_) {
            threepp::Quaternion q;
            q.copy(leftRest_).multiply(delta);
            leftEye_->quaternion.copy(q);
        }
        if (rightEye_) {
            threepp::Quaternion q;
            q.copy(rightRest_).multiply(delta);
            rightEye_->quaternion.copy(q);
        }
    }

    // -----------------------------------------------------------------------
    // VRMNodeConstraint (VRMC_node_constraint)。式は three-vrm 実装に準拠。
    // -----------------------------------------------------------------------
    void VRMNodeConstraint::setInitState() {
        if (!dst) return;
        dstRest.copy(dst->quaternion);
        invDstRest.copy(dstRest).invert();
        if (src) {
            invSrcRest.copy(src->quaternion).invert();
            invSrcRestMulDstRest.copy(src->quaternion).invert().multiply(dstRest);
        }
    }

    void VRMNodeConstraint::update() {
        if (!dst || !src) return;
        if (type == Type::Rotation) {
            // dst = slerp(dstRest, dstRest * (invSrcRest * srcCurrent), weight)
            threepp::Quaternion srcDelta;
            srcDelta.copy(invSrcRest).multiply(src->quaternion);
            threepp::Quaternion target;
            target.copy(dstRest).multiply(srcDelta);
            dst->quaternion.copy(dstRest).slerp(target, weight);
        } else if (type == Type::Roll) {
            // t = invDstRest * srcCurrent * (invSrcRest * dstRest)
            threepp::Quaternion t;
            t.copy(invDstRest).multiply(src->quaternion).multiply(invSrcRestMulDstRest);
            // ロール軸まわりの成分のみ抽出: e = t*axis, fromTo = (e→axis)
            threepp::Vector3 e;
            e.copy(axis).applyQuaternion(t);
            threepp::Quaternion fromTo;
            fromTo.setFromUnitVectors(e, axis);
            // target = dstRest * fromTo * t
            threepp::Quaternion target;
            target.copy(fromTo).premultiply(dstRest).multiply(t);
            dst->quaternion.copy(dstRest).slerp(target, weight);
        } else { // Aim: dst の aim軸を src へ向ける
            dst->updateWorldMatrix(true, false);
            src->updateWorldMatrix(true, false);
            threepp::Quaternion parentWorld, invParentWorld;
            if (dst->parent) {
                dst->parent->getWorldQuaternion(parentWorld);
                invParentWorld.copy(parentWorld).invert();
            }
            threepp::Vector3 n;// rest の aim軸 (world)
            n.copy(axis).applyQuaternion(dstRest).applyQuaternion(parentWorld);
            threepp::Vector3 srcPos, dstPos;
            src->getWorldPosition(srcPos);
            dst->getWorldPosition(dstPos);
            threepp::Vector3 dir;
            dir.copy(srcPos).sub(dstPos).normalize();
            threepp::Quaternion fromTo;
            fromTo.setFromUnitVectors(n, dir);
            // target = invParentWorld * fromTo * parentWorld * dstRest
            threepp::Quaternion target;
            target.copy(fromTo).premultiply(invParentWorld).multiply(parentWorld).multiply(dstRest);
            dst->quaternion.copy(dstRest).slerp(target, weight);
        }
    }

    // -----------------------------------------------------------------------
    // VRMC_springBone (揺れもの物理)。式は three-vrm 準拠。center 空間は未対応(world)。
    // -----------------------------------------------------------------------
    void VRMSpringBoneManager::setInitState() {
        for (auto& j : joints) {
            if (!j.bone) continue;
            j.bone->updateMatrix();
            j.initialLocalMatrix.copy(*j.bone->matrix);
            j.initialLocalRotation.copy(j.bone->quaternion);
            if (j.child) {
                j.initialLocalChildPosition.copy(j.child->position);
            } else {
                // 末端 (子なし): bone の local 位置方向へ 7cm の仮想 tail
                j.initialLocalChildPosition.copy(j.bone->position).normalize().multiplyScalar(0.07f);
            }
            j.boneAxis.copy(j.initialLocalChildPosition).normalize();

            threepp::Vector3 worldTail;
            worldTail.copy(j.initialLocalChildPosition).applyMatrix4(*j.bone->matrixWorld);
            threepp::Vector3 head;
            head.setFromMatrixPosition(*j.bone->matrixWorld);
            threepp::Vector3 diff;
            diff.subVectors(worldTail, head);
            j.worldSpaceBoneLength = diff.length();
            // tail は center 空間で保持 (center==null なら identity=world と同じ)
            if (j.center) {
                threepp::Matrix4 w2c;
                w2c.copy(*j.center->matrixWorld).invert();
                worldTail.applyMatrix4(w2c);
            }
            j.currentTail.copy(worldTail);
            j.prevTail.copy(worldTail);
        }
    }

    void VRMSpringBoneManager::collide(const VRMSpringJoint& j, threepp::Vector3& tail, const threepp::Vector3& head) {
        for (int gi : j.colliderGroupIndices) {
            if (gi < 0 || gi >= static_cast<int>(colliderGroups.size())) continue;
            for (int ci : colliderGroups[gi]) {
                if (ci < 0 || ci >= static_cast<int>(colliders.size())) continue;
                const auto& col = colliders[ci];
                if (!col.node) continue;

                threepp::Vector3 c0;
                c0.copy(col.offset).applyMatrix4(*col.node->matrixWorld);
                threepp::Vector3 nearest;
                if (col.capsule) {
                    threepp::Vector3 c1;
                    c1.copy(col.tail).applyMatrix4(*col.node->matrixWorld);
                    threepp::Vector3 ab;
                    ab.subVectors(c1, c0);
                    const float abLen2 = ab.lengthSq();
                    float t = 0.f;
                    if (abLen2 > 1e-10f) {
                        threepp::Vector3 at;
                        at.subVectors(tail, c0);
                        t = at.dot(ab) / abLen2;
                        t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
                    }
                    nearest.copy(ab).multiplyScalar(t).add(c0);
                } else {
                    nearest.copy(c0);
                }
                threepp::Vector3 delta;
                delta.subVectors(tail, nearest);
                const float dist = delta.length();
                const float minDist = col.radius * colliderScale + j.hitRadius;
                if (dist < minDist && dist > 1e-8f) {
                    threepp::Vector3 dir;
                    dir.copy(delta).multiplyScalar(1.f / dist);
                    tail.copy(nearest).addScaledVector(dir, minDist);
                    // ボーン長で再拘束
                    threepp::Vector3 d;
                    d.subVectors(tail, head).normalize().multiplyScalar(j.worldSpaceBoneLength);
                    tail.copy(head).add(d);
                }
            }
        }
    }

    void VRMSpringBoneManager::updateJoint(VRMSpringJoint& j, float dt) {
        if (!j.bone) return;
        auto& bone = *j.bone;

        threepp::Matrix4 parentWorld;// 既定は identity
        if (bone.parent) parentWorld.copy(*bone.parent->matrixWorld);

        threepp::Vector3 head;
        head.setFromMatrixPosition(*bone.matrixWorld);
        threepp::Vector3 worldTail;
        if (j.child) worldTail.setFromMatrixPosition(*j.child->matrixWorld);
        else worldTail.copy(j.initialLocalChildPosition).applyMatrix4(*bone.matrixWorld);
        threepp::Vector3 diff;
        diff.subVectors(worldTail, head);
        j.worldSpaceBoneLength = diff.length();

        // rest のボーン方向 (world)
        threepp::Vector3 e;
        e.copy(j.boneAxis).transformDirection(j.initialLocalMatrix).transformDirection(parentWorld);

        // center 空間の変換行列 (center==null なら identity)
        threepp::Matrix4 c2w, w2c;
        if (j.center) {
            c2w.copy(*j.center->matrixWorld);
            w2c.copy(c2w).invert();
        }
        // nextTail = center空間で (currentTail + 慣性) → world 変換 → 剛性 + 重力
        // (currentTail/prevTail は center 空間で保持。center の並進で慣性が発生しない)
        threepp::Vector3 nextTail;
        nextTail.copy(j.currentTail);
        threepp::Vector3 inertia;
        inertia.subVectors(j.currentTail, j.prevTail).multiplyScalar(1.f - j.dragForce);
        nextTail.add(inertia);
        nextTail.applyMatrix4(c2w);// center → world
        nextTail.addScaledVector(e, j.stiffness * dt);
        nextTail.addScaledVector(j.gravityDir, j.gravityPower * dt);

        // 長さ拘束
        {
            threepp::Vector3 d;
            d.subVectors(nextTail, head).normalize().multiplyScalar(j.worldSpaceBoneLength);
            nextTail.copy(head).add(d);
        }
        // 衝突
        collide(j, nextTail, head);

        // currentTail/prevTail は center 空間で保持する (nextTail は world)
        j.prevTail.copy(j.currentTail);
        threepp::Vector3 nc;
        nc.copy(nextTail).applyMatrix4(w2c);// world → center
        j.currentTail.copy(nc);

        // 回転: bone.quaternion = initialLocalRotation * fromTo(boneAxis, localTail)
        threepp::Matrix4 m;
        m.multiplyMatrices(parentWorld, j.initialLocalMatrix).invert();
        threepp::Vector3 localTail;
        localTail.copy(nextTail).applyMatrix4(m).normalize();
        threepp::Quaternion fromTo;
        fromTo.setFromUnitVectors(j.boneAxis, localTail);
        bone.quaternion.copy(j.initialLocalRotation).multiply(fromTo);

        // 子 joint が最新を読めるよう world 行列を更新
        bone.updateMatrix();
        bone.matrixWorld->multiplyMatrices(parentWorld, *bone.matrix);
    }

    void VRMSpringBoneManager::update(float dt) {
        if (dt <= 0.f) dt = 1.f / 60.f;// 0 のとき止まらないよう既定
        if (dt > 0.1f) dt = 0.1f;      // 発散防止に上限
        for (auto& j : joints) updateJoint(j, dt);
    }

    // -----------------------------------------------------------------------
    // VRM
    // -----------------------------------------------------------------------
    void VRM::updateConstraints() {
        for (auto& c : constraints) c.update();
    }

    void VRM::update(float deltaSec) {
        // ポーズ(正規化rig転写) → 制約 → 視線 → 表情 → springBone。
        // lookAt(expression 方式)は lookLeft/Right/Up/Down 表情ウェイトを設定するので、
        // その後 applyToScene が morph/textureTransformBind を反映する順にする。
        humanoid.updateRig();
        updateConstraints();// VRMC_node_constraint (roll/aim/rotation)
        lookAt.update();
        expressionManager.applyToScene();
        if (!springBone.empty()) {
            if (scene) scene->updateMatrixWorld(true);// 揺れ物理は最新 world 行列が要る
            springBone.update(deltaSec);
        }
        // MToon UV アニメーション: scroll offset / rotation phase を時間で進める
        if (!uvAnims.empty()) {
            uvAnimTime += deltaSec;
            for (auto& a : uvAnims) {
                if (!a.material) continue;
                const float ox = std::fmod(a.scrollX * uvAnimTime, 1.f);
                const float oy = std::fmod(a.scrollY * uvAnimTime, 1.f);
                const float rot = std::fmod(a.rotSpeed * uvAnimTime, 6.2831853f);
                a.material->uniforms.at("uvOffset").setValue(threepp::Vector2(ox, oy));
                a.material->uniforms.at("uvRotPhase").setValue(rot);
                a.material->uniformsNeedUpdate = true;
            }
        }
    }

    // -----------------------------------------------------------------------
    // VRMLoader
    // -----------------------------------------------------------------------
    std::shared_ptr<VRM> VRMLoader::load(const std::filesystem::path& path) {
        threepp::GLTFLoader loader;
        auto result = loader.load(path);
        if (!result) std::cerr << "[VRM] GLTF load failed: " << path << "\n";
        return buildVRM(result, path.string());
    }

    std::shared_ptr<VRM> VRMLoader::loadFromMemory(const unsigned char* data, std::size_t size) {
        threepp::GLTFLoader loader;
        auto result = loader.load(data, size);
        if (!result) std::cerr << "[VRM] GLTF load failed (memory)\n";
        return buildVRM(result, "<memory>");
    }

    // GLTFResult(=生 glTF)を VRM として解釈する共通処理。load / loadFromMemory 兼用。
    std::shared_ptr<VRM> VRMLoader::buildVRM(std::optional<threepp::GLTFResult>& result,
                                             const std::string& srcLabel) {
        if (!result) {
            return nullptr;
        }
        if (result->json.empty()) {
            std::cerr << "[VRM] no raw glTF JSON exposed (threepp GLTFLoader too old?)\n";
            return nullptr;
        }

        nlohmann::json gltf;
        try {
            gltf = nlohmann::json::parse(result->json);
        } catch (const std::exception& e) {
            std::cerr << "[VRM] JSON parse error: " << e.what() << "\n";
            return nullptr;
        }

        const bool hasExts = gltf.contains("extensions");
        const bool isVrm1 = hasExts && gltf["extensions"].contains("VRMC_vrm");
        const bool isVrm0 = hasExts && gltf["extensions"].contains("VRM");
        if (!isVrm1 && !isVrm0) {
            std::cerr << "[VRM] no VRM/VRMC_vrm extension (not a VRM file): " << srcLabel << "\n";
            return nullptr;
        }

        auto vrm = std::make_shared<VRM>();
        vrm->scene = result->scene;

        // node index -> Bone を対応表から引くヘルパ (VRM 0.x/1.0 共通)
        auto bindBone = [&](const std::string& boneName, int nodeIdx) {
            auto nit = result->nodes.find(nodeIdx);
            if (nit == result->nodes.end()) return;
            auto bone = std::dynamic_pointer_cast<threepp::Bone>(nit->second);
            if (bone) vrm->humanoid.rawBones_[boneName] = bone;
        };

        // glTF mesh index -> その mesh から生成された threepp::Mesh 群 (各 primitive)。
        auto meshesForMeshIdx = [&](int meshIdx) {
            std::vector<std::shared_ptr<threepp::Mesh>> ms;
            for (const auto& [key, mptr] : result->meshPrimitives)
                if (key.first == meshIdx && mptr) ms.push_back(mptr);
            return ms;
        };
        // node index -> mesh index (無ければ -1)
        auto meshIdxOfNode = [&](int nodeIdx) -> int {
            if (!gltf.contains("nodes") || nodeIdx < 0 ||
                nodeIdx >= (int)gltf["nodes"].size())
                return -1;
            return gltf["nodes"][nodeIdx].value("mesh", -1);
        };

        if (isVrm1) {
            // ---- VRM 1.0 (VRMC_vrm) ----
            const auto& vrmExt = gltf["extensions"]["VRMC_vrm"];

            // Meta: infosia/VRM.h でデシリアライズ (meta サブオブジェクトだけ隔離)
            if (vrmExt.contains("meta")) {
                try {
                    VRMC_VRM_1_0::Meta m;
                    VRMC_VRM_1_0::from_json(vrmExt["meta"], m);
                    vrm->meta.name = m.name;
                    vrm->meta.version = m.version;
                    vrm->meta.authors = m.authors;
                    vrm->meta.licenseUrl = m.licenseUrl;
                    vrm->meta.copyrightInformation = m.copyrightInformation;
                } catch (const std::exception& e) {
                    std::cerr << "[VRM] meta parse warning: " << e.what() << "\n";
                }
            }

            // Humanoid: humanBones はオブジェクト { boneName: { node } }。
            // (VRM.h の HumanBones は名前付きフィールドで node 未指定 0 と区別不能なので
            //  生 JSON の存在キーを走査する。)
            if (vrmExt.contains("humanoid") && vrmExt["humanoid"].contains("humanBones")) {
                const auto& humanBones = vrmExt["humanoid"]["humanBones"];
                for (auto it = humanBones.begin(); it != humanBones.end(); ++it) {
                    if (it.value().contains("node"))
                        bindBone(it.key(), it.value()["node"].get<int>());
                }
            }

            // Expressions: expressions.preset / .custom = { name: { morphTargetBinds:[{node,index,weight}] } }
            if (vrmExt.contains("expressions")) {
                const auto& exprs = vrmExt["expressions"];
                auto addGroup = [&](const nlohmann::json& group) {
                    for (auto it = group.begin(); it != group.end(); ++it) {
                        VRMExpressionManager::Expression e;
                        const auto& ex = it.value();
                        e.isBinary = ex.value("isBinary", false);
                        auto parseOverride = [](const std::string& s) {
                            if (s == "block") return VRMExpressionManager::Override::Block;
                            if (s == "blend") return VRMExpressionManager::Override::Blend;
                            return VRMExpressionManager::Override::None;
                        };
                        e.overrideBlink = parseOverride(ex.value("overrideBlink", std::string("none")));
                        e.overrideLookAt = parseOverride(ex.value("overrideLookAt", std::string("none")));
                        e.overrideMouth = parseOverride(ex.value("overrideMouth", std::string("none")));
                        if (ex.contains("morphTargetBinds")) {
                            for (const auto& b : ex["morphTargetBinds"]) {
                                if (!b.contains("node") || !b.contains("index")) continue;
                                const int meshIdx = meshIdxOfNode(b["node"].get<int>());
                                if (meshIdx < 0) continue;
                                const int idx = b["index"].get<int>();
                                const float w = b.value("weight", 1.0f);
                                for (auto& m : meshesForMeshIdx(meshIdx))
                                    e.morphBinds.push_back({m, idx, w});
                            }
                        }
                        // textureTransformBind: material の UV offset/scale を動かす(瞳移動等)。
                        // MToon 材生成後に resolveTexBinds で material を解決する。
                        if (ex.contains("textureTransformBinds")) {
                            for (const auto& b : ex["textureTransformBinds"]) {
                                if (!b.contains("material")) continue;
                                VRMExpressionManager::TexBindRaw raw;
                                raw.matIndex = b["material"].get<int>();
                                if (b.contains("offset") && b["offset"].is_array() && b["offset"].size() >= 2)
                                    raw.offset.set(b["offset"][0].get<float>(), b["offset"][1].get<float>());
                                if (b.contains("scale") && b["scale"].is_array() && b["scale"].size() >= 2)
                                    raw.scale.set(b["scale"][0].get<float>(), b["scale"][1].get<float>());
                                e.texBindsRaw.push_back(raw);
                            }
                        }
                        vrm->expressionManager.expressions_[it.key()] = std::move(e);
                    }
                };
                if (exprs.contains("preset")) addGroup(exprs["preset"]);
                if (exprs.contains("custom")) addGroup(exprs["custom"]);
            }

            // LookAt(1.0): type=="bone"(eye ボーン回転) / "expression"(lookLeft 等の表情駆動)。
            if (vrmExt.contains("lookAt")) {
                const auto& la = vrmExt["lookAt"];
                const std::string type = la.value("type", std::string("bone"));
                auto& L = vrm->lookAt;
                L.head_ = vrm->humanoid.getRawBoneNode("head");
                if (type == "expression") {
                    L.expressionMode_ = true;
                    L.exprMgr_ = &vrm->expressionManager;
                } else {
                    L.leftEye_ = vrm->humanoid.getRawBoneNode("leftEye");
                    L.rightEye_ = vrm->humanoid.getRawBoneNode("rightEye");
                    if (L.leftEye_) L.leftRest_.copy(L.leftEye_->quaternion);
                    if (L.rightEye_) L.rightRest_.copy(L.rightEye_->quaternion);
                }
                if (la.contains("offsetFromHeadBone") && la["offsetFromHeadBone"].is_array() &&
                    la["offsetFromHeadBone"].size() == 3) {
                    const auto& o = la["offsetFromHeadBone"];
                    L.offset_.set(o[0].get<float>(), o[1].get<float>(), o[2].get<float>());
                }
                auto rmap = [&](const char* key, float& inMax, float& outScale, float defOut) {
                    if (la.contains(key)) {
                        inMax = la[key].value("inputMaxValue", 90.0f);
                        outScale = la[key].value("outputScale", defOut);
                    }
                };
                // expression 方式は outputScale がウェイト(既定1)、bone 方式は角度(既定10)。
                const float defOut = (type == "expression") ? 1.0f : 10.0f;
                float dummy = 90.f;
                rmap("rangeMapHorizontalOuter", L.inMaxH_, L.outH_, defOut);
                rmap("rangeMapVerticalUp", L.inMaxV_, L.outVUp_, defOut);
                rmap("rangeMapVerticalDown", dummy, L.outVDown_, defOut);
            }
        } else {
            // ---- VRM 0.x (VRM) ---- Phase0 検証用の最小読み取り (Phase3 で軸/tail 等を拡充)
            const auto& vrmExt = gltf["extensions"]["VRM"];

            // Meta: title / author / version / *LicenseUrl
            if (vrmExt.contains("meta")) {
                const auto& m = vrmExt["meta"];
                vrm->meta.name = m.value("title", std::string());
                vrm->meta.version = m.value("version", std::string());
                if (m.contains("author") && m["author"].is_string())
                    vrm->meta.authors.push_back(m["author"].get<std::string>());
                vrm->meta.licenseUrl = m.value("otherPermissionUrl",
                                               m.value("otherLicenseUrl", std::string()));
            }

            // Humanoid: humanBones は配列 [{ bone, node }]。ボーン名は 1.0 と同じ camelCase。
            if (vrmExt.contains("humanoid") && vrmExt["humanoid"].contains("humanBones")) {
                for (const auto& hb : vrmExt["humanoid"]["humanBones"]) {
                    if (hb.contains("bone") && hb.contains("node") && hb["bone"].is_string())
                        bindBone(hb["bone"].get<std::string>(), hb["node"].get<int>());
                }
            }

            // Expressions(0.x): blendShapeMaster.blendShapeGroups
            //   [{ name, presetName, isBinary, binds:[{mesh,index,weight(0-100)}] }]
            if (vrmExt.contains("blendShapeMaster") &&
                vrmExt["blendShapeMaster"].contains("blendShapeGroups")) {
                for (const auto& g : vrmExt["blendShapeMaster"]["blendShapeGroups"]) {
                    std::string name = g.value("presetName", std::string());
                    if (name.empty() || name == "unknown") name = g.value("name", std::string());
                    if (name.empty()) continue;
                    for (auto& c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

                    VRMExpressionManager::Expression e;
                    e.isBinary = g.value("isBinary", false);
                    if (g.contains("binds")) {
                        for (const auto& b : g["binds"]) {
                            if (!b.contains("mesh") || !b.contains("index")) continue;
                            const int meshIdx = b["mesh"].get<int>();
                            const int idx = b["index"].get<int>();
                            const float w = b.value("weight", 100.0f) / 100.0f;// 0.x は 0-100
                            for (auto& m : meshesForMeshIdx(meshIdx))
                                e.morphBinds.push_back({m, idx, w});
                        }
                    }
                    vrm->expressionManager.expressions_[name] = std::move(e);
                }
            }

            // LookAt(0.x): firstPerson.lookAtTypeName=="Bone" のみ対応。
            if (vrmExt.contains("firstPerson") &&
                vrmExt["firstPerson"].value("lookAtTypeName", std::string("Bone")) == "Bone") {
                const auto& fp = vrmExt["firstPerson"];
                auto& L = vrm->lookAt;
                L.head_ = vrm->humanoid.getRawBoneNode("head");
                L.leftEye_ = vrm->humanoid.getRawBoneNode("leftEye");
                L.rightEye_ = vrm->humanoid.getRawBoneNode("rightEye");
                if (L.leftEye_) L.leftRest_.copy(L.leftEye_->quaternion);
                if (L.rightEye_) L.rightRest_.copy(L.rightEye_->quaternion);
                if (fp.contains("firstPersonBoneOffset")) {
                    const auto& o = fp["firstPersonBoneOffset"];
                    L.offset_.set(o.value("x", 0.f), o.value("y", 0.f), o.value("z", 0.f));
                }
                auto curve = [&](const char* key, float& inMax, float& outScale) {
                    if (fp.contains(key)) {
                        inMax = fp[key].value("xRange", 90.0f);
                        outScale = fp[key].value("yRange", 10.0f);
                    }
                };
                float dummy = 90.f;
                curve("lookAtHorizontalOuter", L.inMaxH_, L.outH_);
                curve("lookAtVerticalUp", L.inMaxV_, L.outVUp_);
                curve("lookAtVerticalDown", dummy, L.outVDown_);
            }
        }

        // VRM 0.x は -Z を正面とする規約。VRM 1.0 (および VRMC_vrm_animation / lookAt 等) は
        // +Z 正面なので、0.x モデルは 180°Y 回転して +Z 正面へ正規化し、規約を 1.0 に揃える。
        // (これをしないと 1.0 の VRMA を 0.x モデルへ適用したとき前後が逆になり不自然になる)
        if (isVrm0 && vrm->scene) {
            vrm->scene->rotateY(3.14159265358979323846f);
        }

        // normalized rig 構築 (world 行列を確定させてから rest world 回転を捕捉)
        if (vrm->scene) {
            vrm->scene->updateMatrixWorld(true);
            vrm->humanoid.buildRig();
        }

        // VRMC_node_constraint (roll/aim/rotation) を各 glTF ノードから収集する。
        // ツイスト/ロールボーンが親等の回転を追従する。rest ポーズで初期状態を捕捉し、
        // src が別制約の dst なら先に処理されるよう依存順にソートする。
        if (gltf.contains("nodes")) {
            const auto& nodes = gltf["nodes"];
            for (int ni = 0; ni < static_cast<int>(nodes.size()); ++ni) {
                const auto& nd = nodes[ni];
                if (!nd.contains("extensions") ||
                    !nd["extensions"].contains("VRMC_node_constraint"))
                    continue;
                const auto& ext = nd["extensions"]["VRMC_node_constraint"];
                if (!ext.contains("constraint")) continue;
                const auto& con = ext["constraint"];

                auto dstIt = result->nodes.find(ni);
                if (dstIt == result->nodes.end()) continue;

                VRMNodeConstraint c;
                c.dst = dstIt->second;
                const nlohmann::json* spec = nullptr;
                if (con.contains("roll")) {
                    c.type = VRMNodeConstraint::Type::Roll;
                    spec = &con["roll"];
                } else if (con.contains("aim")) {
                    c.type = VRMNodeConstraint::Type::Aim;
                    spec = &con["aim"];
                } else if (con.contains("rotation")) {
                    c.type = VRMNodeConstraint::Type::Rotation;
                    spec = &con["rotation"];
                } else {
                    continue;
                }
                const int srcIdx = spec->value("source", -1);
                auto srcIt = result->nodes.find(srcIdx);
                if (srcIt == result->nodes.end()) continue;
                c.src = srcIt->second;
                c.weight = spec->value("weight", 1.0f);
                if (c.type == VRMNodeConstraint::Type::Roll) {
                    const std::string ax = spec->value("rollAxis", std::string("X"));
                    c.axis.set(ax == "X" ? 1.f : 0.f, ax == "Y" ? 1.f : 0.f, ax == "Z" ? 1.f : 0.f);
                } else if (c.type == VRMNodeConstraint::Type::Aim) {
                    const std::string ax = spec->value("aimAxis", std::string("PositiveX"));
                    c.axis.set(ax == "PositiveX" ? 1.f : (ax == "NegativeX" ? -1.f : 0.f),
                               ax == "PositiveY" ? 1.f : (ax == "NegativeY" ? -1.f : 0.f),
                               ax == "PositiveZ" ? 1.f : (ax == "NegativeZ" ? -1.f : 0.f));
                }
                vrm->constraints.push_back(std::move(c));
            }

            // rest ポーズで初期状態を捕捉
            for (auto& c : vrm->constraints) c.setInitState();

            // 依存順(src が別制約の dst なら先)にトポロジカルソート
            if (vrm->constraints.size() > 1) {
                std::unordered_map<threepp::Object3D*, int> dstToIdx;
                for (int i = 0; i < static_cast<int>(vrm->constraints.size()); ++i)
                    dstToIdx[vrm->constraints[i].dst.get()] = i;
                std::vector<VRMNodeConstraint> ordered;
                std::vector<int> state(vrm->constraints.size(), 0);// 0=未,1=処理中,2=済
                std::function<void(int)> visit = [&](int i) {
                    if (state[i] != 0) return;
                    state[i] = 1;
                    auto sit = dstToIdx.find(vrm->constraints[i].src.get());
                    if (sit != dstToIdx.end() && sit->second != i && state[sit->second] == 0)
                        visit(sit->second);
                    state[i] = 2;
                    ordered.push_back(vrm->constraints[i]);
                };
                for (int i = 0; i < static_cast<int>(vrm->constraints.size()); ++i) visit(i);
                vrm->constraints = std::move(ordered);
            }
            if (!vrm->constraints.empty())
                std::cerr << "[VRM] node constraints: " << vrm->constraints.size() << "\n";
        }

        // VRMC_springBone (揺れもの): colliders / colliderGroups / springs(joints)
        if (gltf.contains("extensions") && gltf["extensions"].contains("VRMC_springBone")) {
            const auto& sb = gltf["extensions"]["VRMC_springBone"];
            auto& mgr = vrm->springBone;

            auto readVec3 = [](const nlohmann::json& a, threepp::Vector3& v) {
                if (a.is_array() && a.size() == 3)
                    v.set(a[0].get<float>(), a[1].get<float>(), a[2].get<float>());
            };

            // colliders (index を colliderGroups と揃えるため node 不明でも placeholder を積む)
            if (sb.contains("colliders")) {
                for (const auto& cd : sb["colliders"]) {
                    VRMSpringCollider col;
                    auto nit = result->nodes.find(cd.value("node", -1));
                    if (nit != result->nodes.end()) {
                        col.node = nit->second;
                        if (cd.contains("shape")) {
                            const auto& sh = cd["shape"];
                            const nlohmann::json* s = nullptr;
                            if (sh.contains("sphere")) { col.capsule = false; s = &sh["sphere"]; }
                            else if (sh.contains("capsule")) { col.capsule = true; s = &sh["capsule"]; }
                            if (s) {
                                if (s->contains("offset")) readVec3((*s)["offset"], col.offset);
                                col.radius = s->value("radius", 0.f);
                                if (col.capsule && s->contains("tail")) readVec3((*s)["tail"], col.tail);
                            }
                        }
                    }
                    mgr.colliders.push_back(std::move(col));
                }
            }
            // colliderGroups
            if (sb.contains("colliderGroups")) {
                for (const auto& cg : sb["colliderGroups"]) {
                    std::vector<int> group;
                    if (cg.contains("colliders"))
                        for (const auto& ci : cg["colliders"]) group.push_back(ci.get<int>());
                    mgr.colliderGroups.push_back(std::move(group));
                }
            }
            // springs (各 joint は次の joint を child とするチェーン)
            if (sb.contains("springs")) {
                for (const auto& sp : sb["springs"]) {
                    std::vector<int> groupIdx;
                    if (sp.contains("colliderGroups"))
                        for (const auto& g : sp["colliderGroups"]) groupIdx.push_back(g.get<int>());
                    std::shared_ptr<threepp::Object3D> centerNode;
                    if (sp.contains("center")) {
                        auto cit = result->nodes.find(sp["center"].get<int>());
                        if (cit != result->nodes.end()) centerNode = cit->second;
                    }
                    if (!sp.contains("joints")) continue;
                    const auto& js = sp["joints"];
                    const int n = static_cast<int>(js.size());
                    for (int i = 0; i < n; ++i) {
                        const auto& jd = js[i];
                        auto bit = result->nodes.find(jd.value("node", -1));
                        if (bit == result->nodes.end()) continue;
                        VRMSpringJoint joint;
                        joint.bone = bit->second;
                        joint.center = centerNode;
                        if (i + 1 < n) {
                            auto cit = result->nodes.find(js[i + 1].value("node", -1));
                            if (cit != result->nodes.end()) joint.child = cit->second;
                        }
                        joint.hitRadius = jd.value("hitRadius", 0.f);
                        joint.stiffness = jd.value("stiffness", 1.f);
                        joint.gravityPower = jd.value("gravityPower", 0.f);
                        joint.dragForce = jd.value("dragForce", 0.4f);
                        if (jd.contains("gravityDir")) readVec3(jd["gravityDir"], joint.gravityDir);
                        joint.colliderGroupIndices = groupIdx;
                        mgr.joints.push_back(std::move(joint));
                    }
                }
            }
            if (!mgr.joints.empty()) {
                if (vrm->scene) vrm->scene->updateMatrixWorld(true);
                mgr.setInitState();
                std::cerr << "[VRM] springBone: joints=" << mgr.joints.size()
                          << " colliders=" << mgr.colliders.size() << "\n";
            }
        }

        // VRM 0.x secondaryAnimation (揺れもの): 1.0 の VRMSpringBoneManager へ流し込む。
        // boneGroups[].bones の各 root から子ボーンを再帰して joint チェーンを作り、group の
        // stiffiness/gravityPower/gravityDir/dragForce/hitRadius を全 joint に適用。collider は球のみ。
        if (gltf.contains("extensions") && gltf["extensions"].contains("VRM") &&
            gltf["extensions"]["VRM"].contains("secondaryAnimation")) {
            const auto& sa = gltf["extensions"]["VRM"]["secondaryAnimation"];
            auto& mgr = vrm->springBone;

            auto readVec3Obj = [](const nlohmann::json& o, threepp::Vector3& v) {
                if (o.is_object())
                    v.set(o.value("x", 0.f), o.value("y", 0.f), o.value("z", 0.f));
            };

            // colliderGroups(0.x): 各 group = { node, colliders:[{offset{x,y,z}, radius}] }。
            // 1.0 の (top-level colliders + colliderGroups=index list) 形へ展開する。
            if (sa.contains("colliderGroups")) {
                for (const auto& cg : sa["colliderGroups"]) {
                    std::vector<int> group;
                    std::shared_ptr<threepp::Object3D> node;
                    auto nit = result->nodes.find(cg.value("node", -1));
                    if (nit != result->nodes.end()) node = nit->second;
                    if (cg.contains("colliders")) {
                        for (const auto& c : cg["colliders"]) {
                            VRMSpringCollider col;
                            col.node = node;
                            col.capsule = false;
                            if (c.contains("offset")) readVec3Obj(c["offset"], col.offset);
                            col.radius = c.value("radius", 0.f);
                            group.push_back(static_cast<int>(mgr.colliders.size()));
                            mgr.colliders.push_back(std::move(col));
                        }
                    }
                    mgr.colliderGroups.push_back(std::move(group));
                }
            }

            // boneGroups(0.x): root bone のサブツリーを再帰して joint 化 (UniVRM 準拠、
            // 各ノードの tail=第1子、末端は仮想 tail)。同一 group の全 joint が同じパラメータを持つ。
            if (sa.contains("boneGroups")) {
                for (const auto& bg : sa["boneGroups"]) {
                    const float stiffness = bg.value("stiffiness", 1.f);
                    const float gravityPower = bg.value("gravityPower", 0.f);
                    const float dragForce = bg.value("dragForce", 0.4f);
                    const float hitRadius = bg.value("hitRadius", 0.02f);
                    threepp::Vector3 gravityDir{0.f, -1.f, 0.f};
                    if (bg.contains("gravityDir")) readVec3Obj(bg["gravityDir"], gravityDir);
                    std::vector<int> groupIdx;
                    if (bg.contains("colliderGroups"))
                        for (const auto& g : bg["colliderGroups"]) groupIdx.push_back(g.get<int>());
                    std::shared_ptr<threepp::Object3D> centerNode;
                    if (bg.contains("center") && bg["center"].is_number_integer() &&
                        bg["center"].get<int>() >= 0) {
                        auto cit = result->nodes.find(bg["center"].get<int>());
                        if (cit != result->nodes.end()) centerNode = cit->second;
                    }

                    // glTF ノードインデックスで階層を辿り、pre-order (親→子) で joint 化。
                    // tail = 第1子ノード。result->nodes から shared_ptr を得る。
                    const bool hasNodes = gltf.contains("nodes");
                    std::function<void(int)> addChain = [&](int idx) {
                        auto bit = result->nodes.find(idx);
                        if (bit == result->nodes.end()) return;
                        VRMSpringJoint joint;
                        joint.bone = bit->second;
                        joint.center = centerNode;
                        const nlohmann::json* children = nullptr;
                        if (hasNodes && idx >= 0 && idx < static_cast<int>(gltf["nodes"].size()) &&
                            gltf["nodes"][idx].contains("children"))
                            children = &gltf["nodes"][idx]["children"];
                        if (children && !children->empty()) {
                            auto cit = result->nodes.find((*children)[0].get<int>());
                            if (cit != result->nodes.end()) joint.child = cit->second;
                        }
                        joint.hitRadius = hitRadius;
                        joint.stiffness = stiffness;
                        joint.gravityPower = gravityPower;
                        joint.dragForce = dragForce;
                        joint.gravityDir.copy(gravityDir);
                        joint.colliderGroupIndices = groupIdx;
                        mgr.joints.push_back(std::move(joint));
                        if (children)
                            for (const auto& c : *children) addChain(c.get<int>());
                    };

                    if (bg.contains("bones"))
                        for (const auto& bn : bg["bones"]) addChain(bn.get<int>());
                }
            }

            if (!mgr.joints.empty()) {
                if (vrm->scene) vrm->scene->updateMatrixWorld(true);
                mgr.setInitState();
                std::cerr << "[VRM] springBone(0.x): joints=" << mgr.joints.size()
                          << " colliders=" << mgr.colliders.size() << "\n";
            }
        }

        // VRMC_materials_mtoon: pbr フォールバック材(MeshStandardMaterial)を
        // MeshToonMaterial(トゥーン)へ差し替える。lit=baseColorFactor、shade=shadeColorFactor を
        // gradientMap の色ランプで表現。テクスチャ/emissive/透明度は元材から引き継ぐ。
        // (outline/rim/matcap/normalScale 等の細部は未対応)
        if (isVrm1 && gltf.contains("materials") && gltf.contains("meshes")) {
            struct MToonParam {
                threepp::Color shade{1.f, 1.f, 1.f};
                float shift = -0.05f, toony = 0.9f;
                bool hasOutline = false;
                threepp::Color outlineColor{0.f, 0.f, 0.f};
                float outlineWidth = 0.f;
                bool hasRim = false;
                threepp::Color rimColor{0.f, 0.f, 0.f};
                float rimPower = 5.f, rimLift = 0.f;
                int matcapTex = -1;
                threepp::Color matcapColor{1.f, 1.f, 1.f};
                float uvScrollX = 0.f, uvScrollY = 0.f, uvRotSpeed = 0.f;
            };
            std::unordered_map<int, MToonParam> mtoon;
            const auto& mats = gltf["materials"];
            for (int mi = 0; mi < static_cast<int>(mats.size()); ++mi) {
                const auto& md = mats[mi];
                if (!md.contains("extensions") || !md["extensions"].contains("VRMC_materials_mtoon")) continue;
                const auto& mt = md["extensions"]["VRMC_materials_mtoon"];
                MToonParam p;
                if (mt.contains("shadeColorFactor") && mt["shadeColorFactor"].is_array() &&
                    mt["shadeColorFactor"].size() >= 3) {
                    const auto& s = mt["shadeColorFactor"];
                    p.shade.setRGB(s[0].get<float>(), s[1].get<float>(), s[2].get<float>());
                }
                p.shift = mt.value("shadingShiftFactor", -0.05f);
                p.toony = mt.value("shadingToonyFactor", 0.9f);
                // outline (輪郭線)。none/未指定は無し。width>0 のときのみ。
                const std::string owm = mt.value("outlineWidthMode", std::string("none"));
                const float ow = mt.value("outlineWidthFactor", 0.f);
                if (owm != "none" && ow > 0.f) {
                    p.hasOutline = true;
                    p.outlineWidth = ow;// worldCoordinates=メートル。screenCoordinatesも近似で同扱い
                    if (mt.contains("outlineColorFactor") && mt["outlineColorFactor"].is_array() &&
                        mt["outlineColorFactor"].size() >= 3) {
                        const auto& oc = mt["outlineColorFactor"];
                        p.outlineColor.setRGB(oc[0].get<float>(), oc[1].get<float>(), oc[2].get<float>());
                    }
                }
                // parametric rim (リムライト)。rimColor が実質黒なら無し。
                if (mt.contains("parametricRimColorFactor") && mt["parametricRimColorFactor"].is_array() &&
                    mt["parametricRimColorFactor"].size() >= 3) {
                    const auto& rc = mt["parametricRimColorFactor"];
                    threepp::Color rcol(rc[0].get<float>(), rc[1].get<float>(), rc[2].get<float>());
                    if (rcol.r + rcol.g + rcol.b > 0.01f) {
                        p.hasRim = true;
                        p.rimColor.copy(rcol);
                        p.rimPower = mt.value("parametricRimFresnelPowerFactor", 5.f);
                        p.rimLift = mt.value("parametricRimLiftFactor", 0.f);
                    }
                }
                // matcap (加算)
                if (mt.contains("matcapTexture") && mt["matcapTexture"].contains("index")) {
                    p.matcapTex = mt["matcapTexture"]["index"].get<int>();
                    if (mt.contains("matcapFactor") && mt["matcapFactor"].is_array() &&
                        mt["matcapFactor"].size() >= 3) {
                        const auto& mf = mt["matcapFactor"];
                        p.matcapColor.setRGB(mf[0].get<float>(), mf[1].get<float>(), mf[2].get<float>());
                    }
                }
                // UV アニメーション (scroll/rotation)
                p.uvScrollX = mt.value("uvAnimationScrollXSpeedFactor", 0.f);
                p.uvScrollY = mt.value("uvAnimationScrollYSpeedFactor", 0.f);
                p.uvRotSpeed = mt.value("uvAnimationRotationSpeedFactor", 0.f);
                mtoon[mi] = p;
            }
            if (!mtoon.empty()) {
                const auto& meshes = gltf["meshes"];
                int swapped = 0, outlined = 0, rimmed = 0;
                std::unordered_map<int, std::shared_ptr<threepp::ShaderMaterial>> matByIdx;// textureTransformBind 解決用
                for (int meshI = 0; meshI < static_cast<int>(meshes.size()); ++meshI) {
                    if (!meshes[meshI].contains("primitives")) continue;
                    const auto& prims = meshes[meshI]["primitives"];
                    for (int primI = 0; primI < static_cast<int>(prims.size()); ++primI) {
                        const int matIdx = prims[primI].value("material", -1);
                        auto pit = mtoon.find(matIdx);
                        if (pit == mtoon.end()) continue;
                        auto meshIt = result->meshPrimitives.find({meshI, primI});
                        if (meshIt == result->meshPrimitives.end() || !meshIt->second) continue;
                        auto mesh = meshIt->second;

                        // 元材の型は VRM だと MeshBasicMaterial(KHR_materials_unlit)か
                        // MeshStandardMaterial のことがあるので、capability mixin で取り出す。
                        auto matPtr = mesh->material();
                        threepp::Material* mat0 = matPtr.get();
                        threepp::Color lit(1.f, 1.f, 1.f), emissive(0.f, 0.f, 0.f);
                        std::shared_ptr<threepp::Texture> map, emissiveMap, normalMap;
                        float emissiveIntensity = 1.f;
                        if (auto* c = dynamic_cast<threepp::MaterialWithColor*>(mat0)) lit.copy(c->color);
                        if (auto* m = dynamic_cast<threepp::MaterialWithMap*>(mat0)) map = m->map;
                        if (auto* e = dynamic_cast<threepp::MaterialWithEmissive*>(mat0)) {
                            emissive.copy(e->emissive);
                            emissiveIntensity = e->emissiveIntensity;
                            emissiveMap = e->emissiveMap;
                        }
                        if (auto* n = dynamic_cast<threepp::MaterialWithNormalMap*>(mat0)) normalMap = n->normalMap;

                        MToonShaderParams sp;
                        sp.litColor.copy(lit);
                        sp.shadeColor.copy(pit->second.shade);
                        sp.shadingShift = pit->second.shift;
                        sp.shadingToony = pit->second.toony;
                        sp.baseMap = map.get();
                        sp.emissive.copy(emissive);
                        sp.emissive.multiplyScalar(emissiveIntensity);
                        sp.emissiveMap = emissiveMap.get();
                        sp.normalMap = normalMap.get();
                        if (pit->second.hasRim) {
                            sp.rimColor.copy(pit->second.rimColor);
                            sp.rimPower = pit->second.rimPower;
                            sp.rimLift = pit->second.rimLift;
                        }
                        if (pit->second.matcapTex >= 0) {
                            auto mcit = result->textures.find(pit->second.matcapTex);
                            if (mcit != result->textures.end() && mcit->second) {
                                sp.matcapMap = mcit->second.get();
                                sp.matcapColor.copy(pit->second.matcapColor);
                                vrm->keepAliveTextures.push_back(mcit->second);
                            }
                        }
                        if (mat0) {
                            sp.transparent = mat0->transparent;
                            sp.opacity = mat0->opacity;
                            sp.alphaTest = mat0->alphaTest;
                            sp.side = mat0->side;
                        }
                        auto toon = makeMToonMaterial(sp);
                        matByIdx[matIdx] = toon;// textureTransformBind 解決用
                        // uniform は raw Texture* 参照。元材差し替えで解放されないよう寿命保持。
                        if (map) vrm->keepAliveTextures.push_back(map);
                        if (emissiveMap) vrm->keepAliveTextures.push_back(emissiveMap);
                        if (normalMap) vrm->keepAliveTextures.push_back(normalMap);
                        // morph を持つ mesh(顔等)は morphTargets を有効化(頂点シェーダに morph include 済み)
                        if (auto geo = mesh->geometry()) {
                            if (geo->getMorphAttribute("position")) {
                                toon->morphTargets = true;
                                if (geo->getMorphAttribute("normal")) toon->morphNormals = true;
                            }
                        }
                        mesh->setMaterial(toon);
                        ++swapped;

                        // UV アニメーション材を登録 (update(dt) で uvOffset/uvRotPhase を更新)
                        if (pit->second.uvScrollX != 0.f || pit->second.uvScrollY != 0.f ||
                            pit->second.uvRotSpeed != 0.f) {
                            vrm->uvAnims.push_back({toon, pit->second.uvScrollX,
                                                    pit->second.uvScrollY, pit->second.uvRotSpeed});
                        }

                        // outline (反転ハル) を重ねる。rim は MToon シェーダに統合済み。
                        // surface と同じアルファ discard のため base map + alphaTest を渡す。
                        if (pit->second.hasOutline) {
                            addMToonOutline(mesh, makeOutlineMaterial(pit->second.outlineColor, pit->second.outlineWidth,
                                                                     map.get(), mat0 ? mat0->alphaTest : 0.f));
                            ++outlined;
                        }
                        if (pit->second.hasRim) ++rimmed;
                    }
                }
                // textureTransformBind の material index を MToon 材へ解決 (瞳移動 UV 等)
                vrm->expressionManager.resolveTexBinds(matByIdx);
                std::cerr << "[VRM] MToon materials applied: " << swapped << " primitives ("
                          << mtoon.size() << " materials), outlines=" << outlined << " rims=" << rimmed
                          << " uvAnims=" << vrm->uvAnims.size() << "\n";
            }
        }

        // VRM 0.x MToon: VRM.materialProperties(shader=="VRM/MToon") を makeMToonMaterial へ。
        // 0.x はテクスチャを index で持つので result->textures から解決する。matcap(_SphereAdd)対応。
        if (!isVrm1 && gltf.contains("extensions") && gltf["extensions"].contains("VRM") &&
            gltf["extensions"]["VRM"].contains("materialProperties") && gltf.contains("meshes")) {
            const auto& mps = gltf["extensions"]["VRM"]["materialProperties"];
            const auto& meshes = gltf["meshes"];
            auto texAt = [&](int idx) -> threepp::Texture* {
                auto it = result->textures.find(idx);
                return it != result->textures.end() ? it->second.get() : nullptr;
            };
            auto keepTex = [&](int idx) {
                auto it = result->textures.find(idx);
                if (it != result->textures.end() && it->second) vrm->keepAliveTextures.push_back(it->second);
            };
            auto getFloat = [](const nlohmann::json& mp, const char* k, float def) {
                const auto& fp = mp.contains("floatProperties") ? mp["floatProperties"] : nlohmann::json::object();
                return fp.contains(k) ? fp[k].get<float>() : def;
            };
            auto getVec = [](const nlohmann::json& mp, const char* k, threepp::Color& c) {
                if (!mp.contains("vectorProperties")) return;
                const auto& vp = mp["vectorProperties"];
                if (vp.contains(k) && vp[k].is_array() && vp[k].size() >= 3)
                    c.setRGB(vp[k][0].get<float>(), vp[k][1].get<float>(), vp[k][2].get<float>());
            };
            auto getTexIdx = [](const nlohmann::json& mp, const char* k) -> int {
                if (!mp.contains("textureProperties")) return -1;
                const auto& tp = mp["textureProperties"];
                return tp.contains(k) ? tp[k].get<int>() : -1;
            };
            int swapped = 0, outlined = 0;
            for (int meshI = 0; meshI < static_cast<int>(meshes.size()); ++meshI) {
                if (!meshes[meshI].contains("primitives")) continue;
                const auto& prims = meshes[meshI]["primitives"];
                for (int primI = 0; primI < static_cast<int>(prims.size()); ++primI) {
                    const int mi = prims[primI].value("material", -1);
                    if (mi < 0 || mi >= static_cast<int>(mps.size())) continue;
                    const auto& mp = mps[mi];
                    if (mp.value("shader", std::string()) != "VRM/MToon") continue;
                    auto meshIt = result->meshPrimitives.find({meshI, primI});
                    if (meshIt == result->meshPrimitives.end() || !meshIt->second) continue;
                    auto mesh = meshIt->second;

                    MToonShaderParams sp;
                    getVec(mp, "_Color", sp.litColor);
                    getVec(mp, "_ShadeColor", sp.shadeColor);
                    getVec(mp, "_EmissionColor", sp.emissive);
                    // 0.x _ShadeShift/_ShadeToony → 1.0 shadingShift/Toony 変換
                    // (three-vrm VRMMaterialsV0CompatPlugin と同一式)。
                    {
                        const float sShift = getFloat(mp, "_ShadeShift", 0.f);
                        float sToony = getFloat(mp, "_ShadeToony", 0.9f);
                        sToony = sToony + (1.0f - sToony) * (0.5f + 0.5f * sShift);// lerp(toony,1,0.5+0.5*shift)
                        sp.shadingToony = sToony;
                        sp.shadingShift = -sShift - (1.0f - sToony);
                    }
                    const int mainTex = getTexIdx(mp, "_MainTex");
                    sp.baseMap = texAt(mainTex);
                    keepTex(mainTex);
                    const int emiTex = getTexIdx(mp, "_EmissionMap");
                    sp.emissiveMap = texAt(emiTex);
                    keepTex(emiTex);
                    const int bump = getTexIdx(mp, "_BumpMap");
                    sp.normalMap = texAt(bump);
                    keepTex(bump);
                    sp.normalScale = getFloat(mp, "_BumpScale", 1.f);
                    const int sphere = getTexIdx(mp, "_SphereAdd");
                    sp.matcapMap = texAt(sphere);
                    keepTex(sphere);
                    threepp::Color rim{0.f, 0.f, 0.f};
                    getVec(mp, "_RimColor", rim);
                    if (rim.r + rim.g + rim.b > 0.01f) {
                        sp.rimColor.copy(rim);
                        sp.rimPower = getFloat(mp, "_RimFresnelPower", 5.f);
                        sp.rimLift = getFloat(mp, "_RimLift", 0.f);
                    }
                    // 透明モードは tagMap.RenderType(常に "Opaque" で当てにならない)ではなく
                    // floatProperties._BlendMode を権威にする (0=不透明,1=cutout,2/3=透明)。
                    const float blendMode = getFloat(mp, "_BlendMode", 0.f);
                    if (blendMode >= 0.5f && blendMode < 1.5f)
                        sp.alphaTest = getFloat(mp, "_Cutoff", 0.5f);// cutout
                    else if (blendMode >= 1.5f)
                        sp.transparent = true;                       // alpha blend

                    auto toon = makeMToonMaterial(sp);
                    if (auto geo = mesh->geometry()) {
                        if (geo->getMorphAttribute("position")) {
                            toon->morphTargets = true;
                            if (geo->getMorphAttribute("normal")) toon->morphNormals = true;
                        }
                    }
                    mesh->setMaterial(toon);
                    ++swapped;
                    // outline: _OutlineWidthMode(0=none) & width>0。0.x width は *0.01 スケール。
                    if (getFloat(mp, "_OutlineWidthMode", 0.f) > 0.5f) {
                        const float ow = getFloat(mp, "_OutlineWidth", 0.f);
                        if (ow > 0.f) {
                            threepp::Color oc{0.f, 0.f, 0.f};
                            getVec(mp, "_OutlineColor", oc);
                            addMToonOutline(mesh, makeOutlineMaterial(oc, ow * 0.01f, sp.baseMap, sp.alphaTest));
                            ++outlined;
                        }
                    }
                }
            }
            if (swapped > 0)
                std::cerr << "[VRM] MToon(0.x) applied: " << swapped << " primitives, outlines=" << outlined << "\n";
        }

        // morph target をレンダリングへ反映させる: morph 属性を持つ mesh の material に
        // morphTargets/morphNormals を立てる (threepp は既定 false のため表情が出ない)。
        // overlay(outline/rim)は独自頂点シェーダの ShaderMaterial なので除外する。
        if (vrm->scene) {
            int morphMats = 0;
            vrm->scene->traverseType<threepp::Mesh>([&](threepp::Mesh& mesh) {
                auto geo = mesh.geometry();
                if (!geo || geo->getMorphAttribute("position") == nullptr) return;
                const bool hasNrm = geo->getMorphAttribute("normal") != nullptr;
                for (auto& m : mesh.materials()) {
                    if (!m || std::dynamic_pointer_cast<threepp::ShaderMaterial>(m)) continue;
                    if (auto mm = m->as<threepp::MaterialWithMorphTargets>()) {
                        mm->morphTargets = true;
                        if (hasNrm) mm->morphNormals = true;
                        ++morphMats;
                    }
                }
            });
            if (morphMats > 0)
                std::cerr << "[VRM] morphTargets enabled on " << morphMats << " materials\n";
        }

        std::cerr << "[VRM] loaded (" << (isVrm1 ? "1.0" : "0.x") << "): bones="
                  << vrm->humanoid.size() << " expressions=" << vrm->expressionManager.size()
                  << " meta.name=\"" << vrm->meta.name << "\"\n";
        return vrm;
    }

    // -----------------------------------------------------------------------
    // VRMAnimationLoader / VRM::applyAnimation
    // -----------------------------------------------------------------------
    std::shared_ptr<VRMAnimation> VRMAnimationLoader::load(const std::filesystem::path& path) {
        threepp::GLTFLoader loader;
        auto result = loader.load(path);
        if (!result) std::cerr << "[VRMA] GLTF load failed: " << path << "\n";
        return buildVRMA(result, path.string());
    }

    std::shared_ptr<VRMAnimation> VRMAnimationLoader::loadFromMemory(const unsigned char* data, std::size_t size) {
        threepp::GLTFLoader loader;
        auto result = loader.load(data, size);
        if (!result) std::cerr << "[VRMA] GLTF load failed (memory)\n";
        return buildVRMA(result, "<memory>");
    }

    std::shared_ptr<VRMAnimation> VRMAnimationLoader::buildVRMA(std::optional<threepp::GLTFResult>& result,
                                                               const std::string& srcLabel) {
        if (!result) return nullptr;
        if (result->json.empty()) return nullptr;

        nlohmann::json gltf;
        try {
            gltf = nlohmann::json::parse(result->json);
        } catch (const std::exception& e) {
            std::cerr << "[VRMA] JSON parse error: " << e.what() << "\n";
            return nullptr;
        }
        if (!gltf.contains("extensions") || !gltf["extensions"].contains("VRMC_vrm_animation")) {
            std::cerr << "[VRMA] no VRMC_vrm_animation extension: " << srcLabel << "\n";
            return nullptr;
        }
        const auto& va = gltf["extensions"]["VRMC_vrm_animation"];

        // VRMA node index -> human bone 名
        std::unordered_map<int, std::string> nodeToBone;
        int hipsNode = -1;
        if (va.contains("humanoid") && va["humanoid"].contains("humanBones")) {
            const auto& hb = va["humanoid"]["humanBones"];
            for (auto it = hb.begin(); it != hb.end(); ++it)
                if (it.value().contains("node")) {
                    const int ni = it.value()["node"].get<int>();
                    nodeToBone[ni] = it.key();
                    if (it.key() == "hips") hipsNode = ni;
                }
        }
        if (nodeToBone.empty()) return nullptr;

        // 表情 node index -> 表情名 (preset/custom)
        std::unordered_map<int, std::string> nodeToExpr;
        if (va.contains("expressions")) {
            for (const char* grp : {"preset", "custom"}) {
                if (!va["expressions"].contains(grp)) continue;
                const auto& g = va["expressions"][grp];
                for (auto it = g.begin(); it != g.end(); ++it)
                    if (it.value().contains("node"))
                        nodeToExpr[it.value()["node"].get<int>()] = it.key();
            }
        }

        auto anim = std::make_shared<VRMAnimation>();
        if (!result->rawAnimations.empty()) {
            const auto& ra = result->rawAnimations.front();
            for (const auto& ch : ra.channels) {
                if (ch.path == "rotation") {
                    auto bit = nodeToBone.find(ch.node);
                    if (bit == nodeToBone.end()) continue;
                    VRMAnimation::RotTrack t;
                    t.times = ch.times;
                    t.values = ch.values;
                    if (!t.times.empty())
                        anim->duration_ = std::max(anim->duration_, t.times.back());
                    anim->boneRotations_[bit->second] = std::move(t);
                } else if (ch.path == "translation") {
                    if (ch.node == hipsNode) {// hips ルートモーション
                        anim->hasHips_ = true;
                        anim->hipsTranslation_.times = ch.times;
                        anim->hipsTranslation_.values = ch.values;
                        if (!ch.times.empty())
                            anim->duration_ = std::max(anim->duration_, ch.times.back());
                    } else {// 表情 (translation.x をウェイトとして採用)
                        auto eit = nodeToExpr.find(ch.node);
                        if (eit == nodeToExpr.end()) continue;
                        VRMAnimation::ScalarTrack t;
                        t.times = ch.times;
                        t.values.reserve(ch.times.size());
                        for (std::size_t k = 0; k < ch.times.size(); ++k)
                            t.values.push_back(ch.values.size() > k * 3 ? ch.values[k * 3] : 0.f);
                        if (!t.times.empty())
                            anim->duration_ = std::max(anim->duration_, t.times.back());
                        anim->expressionTracks_[eit->second] = std::move(t);
                    }
                }
            }
        }

        // VRMA hips の rest 位置 (ローカル) と rest world Y を glTF ノード階層から算出。
        // 高さスケール = model_hipsRestWorldY / vrmaHipsRestWorldY で hips 移動量を補正する。
        if (anim->hasHips_ && hipsNode >= 0 && gltf.contains("nodes")) {
            const auto& nodes = gltf["nodes"];
            std::unordered_map<int, int> childToParent;
            for (int i = 0; i < static_cast<int>(nodes.size()); ++i)
                if (nodes[i].contains("children"))
                    for (const auto& c : nodes[i]["children"])
                        childToParent[c.get<int>()] = i;
            auto localT = [&](int idx) -> threepp::Vector3 {
                threepp::Vector3 v;
                if (nodes[idx].contains("translation")) {
                    const auto& t = nodes[idx]["translation"];
                    v.set(t[0].get<float>(), t[1].get<float>(), t[2].get<float>());
                }
                return v;
            };
            anim->vrmaHipsRest_.copy(localT(hipsNode));
            float y = 0.f;
            int idx = hipsNode;
            while (idx >= 0) {
                y += localT(idx).y;
                auto pit = childToParent.find(idx);
                idx = (pit != childToParent.end()) ? pit->second : -1;
            }
            anim->vrmaHipsRestY_ = y;
        }

        std::cerr << "[VRMA] loaded: boneTracks=" << anim->boneTrackCount()
                  << " exprTracks=" << anim->expressionTrackCount()
                  << " hips=" << (anim->hasHips_ ? 1 : 0)
                  << " vrmaHipsY=" << anim->vrmaHipsRestY_
                  << " duration=" << anim->duration() << "\n";
        return anim;
    }

    void VRM::applyAnimation(const VRMAnimation& anim, float time) {
        for (const auto& [boneName, track] : anim.boneRotations_) {
            auto node = humanoid.getNormalizedBoneNode(boneName);
            if (!node || track.times.empty()) continue;

            const auto& T = track.times;
            const auto& V = track.values;// xyzw per key
            const std::size_t n = T.size();

            float qx, qy, qz, qw;
            if (time <= T.front()) {
                qx = V[0]; qy = V[1]; qz = V[2]; qw = V[3];
            } else if (time >= T.back()) {
                const std::size_t k = n - 1;
                qx = V[k * 4]; qy = V[k * 4 + 1]; qz = V[k * 4 + 2]; qw = V[k * 4 + 3];
            } else {
                std::size_t i1 = 1;
                while (i1 < n && T[i1] < time) ++i1;
                const std::size_t i0 = i1 - 1;
                const float span = T[i1] - T[i0];
                const float a = span > 1e-8f ? (time - T[i0]) / span : 0.f;
                float x0 = V[i0 * 4], y0 = V[i0 * 4 + 1], z0 = V[i0 * 4 + 2], w0 = V[i0 * 4 + 3];
                float x1 = V[i1 * 4], y1 = V[i1 * 4 + 1], z1 = V[i1 * 4 + 2], w1 = V[i1 * 4 + 3];
                // 最短弧 (符号合わせ) の後 nlerp
                if (x0 * x1 + y0 * y1 + z0 * z1 + w0 * w1 < 0.f) {
                    x1 = -x1; y1 = -y1; z1 = -z1; w1 = -w1;
                }
                qx = x0 + (x1 - x0) * a; qy = y0 + (y1 - y0) * a;
                qz = z0 + (z1 - z0) * a; qw = w0 + (w1 - w0) * a;
                const float len = std::sqrt(qx * qx + qy * qy + qz * qz + qw * qw);
                if (len > 1e-8f) { qx /= len; qy /= len; qz /= len; qw /= len; }
            }
            node->quaternion.set(qx, qy, qz, qw);
        }

        // --- hips ルートモーション ---
        if (anim.hasHips_ && humanoid.hasHips()) {
            if (auto hipsNorm = humanoid.getNormalizedBoneNode("hips")) {
                const auto& T = anim.hipsTranslation_.times;
                const auto& V = anim.hipsTranslation_.values;// xyz per key
                if (!T.empty()) {
                    const std::size_t n = T.size();
                    float sx, sy, sz;
                    if (time <= T.front()) {
                        sx = V[0]; sy = V[1]; sz = V[2];
                    } else if (time >= T.back()) {
                        const std::size_t k = n - 1;
                        sx = V[k * 3]; sy = V[k * 3 + 1]; sz = V[k * 3 + 2];
                    } else {
                        std::size_t i1 = 1;
                        while (i1 < n && T[i1] < time) ++i1;
                        const std::size_t i0 = i1 - 1;
                        const float span = T[i1] - T[i0];
                        const float a = span > 1e-8f ? (time - T[i0]) / span : 0.f;
                        sx = V[i0 * 3] + (V[i1 * 3] - V[i0 * 3]) * a;
                        sy = V[i0 * 3 + 1] + (V[i1 * 3 + 1] - V[i0 * 3 + 1]) * a;
                        sz = V[i0 * 3 + 2] + (V[i1 * 3 + 2] - V[i0 * 3 + 2]) * a;
                    }
                    // 高さスケール補正した上で VRMA hips rest からの offset を model rest へ加算
                    const float scale = anim.vrmaHipsRestY_ > 1e-4f
                            ? humanoid.hipsRestWorldY() / anim.vrmaHipsRestY_ : 1.f;
                    threepp::Vector3 rest = humanoid.hipsRestNormPos();
                    hipsNorm->position.set(
                            rest.x + (sx - anim.vrmaHipsRest_.x) * scale,
                            rest.y + (sy - anim.vrmaHipsRest_.y) * scale,
                            rest.z + (sz - anim.vrmaHipsRest_.z) * scale);
                }
            }
        }

        // --- 表情トラック (VRMA に含まれる場合のみ。anim.vrma には無し) ---
        for (const auto& [name, track] : anim.expressionTracks_) {
            if (track.times.empty()) continue;
            const auto& T = track.times;
            const auto& V = track.values;
            const std::size_t n = T.size();
            float w;
            if (time <= T.front()) w = V.front();
            else if (time >= T.back()) w = V.back();
            else {
                std::size_t i1 = 1;
                while (i1 < n && T[i1] < time) ++i1;
                const std::size_t i0 = i1 - 1;
                const float span = T[i1] - T[i0];
                const float a = span > 1e-8f ? (time - T[i0]) / span : 0.f;
                w = V[i0] + (V[i1] - V[i0]) * a;
            }
            expressionManager.setValue(name, w);
        }
    }

}// namespace krkrvrm
