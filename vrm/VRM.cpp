#include "VRM.hpp"

#include <threepp/loaders/GLTFLoader.hpp>

// VRM.h は nlohmann::json を前提にするので先に include する。
#include <nlohmann/json.hpp>

#ifndef USE_VRMC_VRM_1_0
#define USE_VRMC_VRM_1_0// 通常は CMake の DEFINITIONS で定義済み
#endif
#include <VRMC/VRM.h>

#include <threepp/math/Euler.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iostream>

namespace krkrvrm {

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

    void VRMExpressionManager::applyToScene() {
        // 1) bind 対象の morph 影響度を 0 クリア
        for (auto& [name, expr] : expressions_) {
            for (auto& b : expr.morphBinds) {
                if (!b.mesh) continue;
                auto& inf = b.mesh->morphTargetInfluences();
                if (b.index >= 0 && static_cast<std::size_t>(b.index) < inf.size())
                    inf[b.index] = 0.f;
            }
        }
        // 2) 各表情ウェイト x bind ウェイトを加算
        for (auto& [name, expr] : expressions_) {
            auto vit = values_.find(name);
            float v = (vit != values_.end()) ? vit->second : 0.f;
            if (v <= 0.f) continue;
            if (expr.isBinary) v = (v >= 0.5f) ? 1.f : 0.f;
            for (auto& b : expr.morphBinds) {
                if (!b.mesh) continue;
                auto& inf = b.mesh->morphTargetInfluences();
                if (b.index >= 0 && static_cast<std::size_t>(b.index) < inf.size())
                    inf[b.index] += v * b.weight;
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

        // rangeMap (線形近似): |角度| を inputMax でクランプ/正規化し outputScale 倍
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
    // VRM
    // -----------------------------------------------------------------------
    void VRM::update(float /*deltaSec*/) {
        // Phase1: ポーズ(正規化rig転写) → 表情 → 視線。Phase2 で springBone を追加。
        humanoid.updateRig();
        expressionManager.applyToScene();
        lookAt.update();
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
                        vrm->expressionManager.expressions_[it.key()] = std::move(e);
                    }
                };
                if (exprs.contains("preset")) addGroup(exprs["preset"]);
                if (exprs.contains("custom")) addGroup(exprs["custom"]);
            }

            // LookAt(1.0): type=="bone" のみ対応。eye ボーンを回転させる。
            if (vrmExt.contains("lookAt") && vrmExt["lookAt"].value("type", std::string("bone")) == "bone") {
                const auto& la = vrmExt["lookAt"];
                auto& L = vrm->lookAt;
                L.head_ = vrm->humanoid.getRawBoneNode("head");
                L.leftEye_ = vrm->humanoid.getRawBoneNode("leftEye");
                L.rightEye_ = vrm->humanoid.getRawBoneNode("rightEye");
                if (L.leftEye_) L.leftRest_.copy(L.leftEye_->quaternion);
                if (L.rightEye_) L.rightRest_.copy(L.rightEye_->quaternion);
                if (la.contains("offsetFromHeadBone") && la["offsetFromHeadBone"].is_array() &&
                    la["offsetFromHeadBone"].size() == 3) {
                    const auto& o = la["offsetFromHeadBone"];
                    L.offset_.set(o[0].get<float>(), o[1].get<float>(), o[2].get<float>());
                }
                auto rmap = [&](const char* key, float& inMax, float& outScale) {
                    if (la.contains(key)) {
                        inMax = la[key].value("inputMaxValue", 90.0f);
                        outScale = la[key].value("outputScale", 10.0f);
                    }
                };
                float dummy = 90.f;
                rmap("rangeMapHorizontalOuter", L.inMaxH_, L.outH_);
                rmap("rangeMapVerticalUp", L.inMaxV_, L.outVUp_);
                rmap("rangeMapVerticalDown", dummy, L.outVDown_);
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

        // normalized rig 構築 (world 行列を確定させてから rest world 回転を捕捉)
        if (vrm->scene) {
            vrm->scene->updateMatrixWorld(true);
            vrm->humanoid.buildRig();
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
        if (va.contains("humanoid") && va["humanoid"].contains("humanBones")) {
            const auto& hb = va["humanoid"]["humanBones"];
            for (auto it = hb.begin(); it != hb.end(); ++it)
                if (it.value().contains("node"))
                    nodeToBone[it.value()["node"].get<int>()] = it.key();
        }
        if (nodeToBone.empty()) return nullptr;

        auto anim = std::make_shared<VRMAnimation>();
        if (!result->rawAnimations.empty()) {
            const auto& ra = result->rawAnimations.front();
            for (const auto& ch : ra.channels) {
                if (ch.path != "rotation") continue;// Phase1: rotation のみ
                auto bit = nodeToBone.find(ch.node);
                if (bit == nodeToBone.end()) continue;
                VRMAnimation::RotTrack t;
                t.times = ch.times;
                t.values = ch.values;
                if (!t.times.empty())
                    anim->duration_ = std::max(anim->duration_, t.times.back());
                anim->boneRotations_[bit->second] = std::move(t);
            }
        }
        std::cerr << "[VRMA] loaded: boneTracks=" << anim->boneTrackCount()
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
    }

}// namespace krkrvrm
