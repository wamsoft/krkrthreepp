#ifndef KRKR_THREEPP_VRM_HPP
#define KRKR_THREEPP_VRM_HPP

// Phase 0 VRM runtime (pure C++, no ncbind).
// See docs/vrm_phase0_design.md. Depends on threepp + infosia/VRM.h + nlohmann.

#include <threepp/objects/Bone.hpp>
#include <threepp/objects/Group.hpp>
#include <threepp/objects/Mesh.hpp>
#include <threepp/math/Quaternion.hpp>
#include <threepp/math/Vector3.hpp>
#include <threepp/loaders/GLTFLoader.hpp>

#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace threepp { class Texture; class ShaderMaterial; }

namespace krkrvrm {

    /// VRM メタ情報 (ライセンス/作者等)。Phase0 は主要フィールドのみ。
    struct VRMMeta {
        std::string name;
        std::string version;
        std::vector<std::string> authors;
        std::string licenseUrl;
        std::string copyrightInformation;
    };

    /// Humanoid。Phase0 は raw ボーン (元モデルのボーンそのもの) の名前引きのみ。
    /// normalized/raw の二重リグは Phase1。
    class VRMHumanoid {
    public:
        /// VRM human bone 名 ("hips","spine",...,"leftUpperArm",...) -> 実 Bone。無ければ nullptr。
        [[nodiscard]] std::shared_ptr<threepp::Bone> getRawBoneNode(const std::string& humanBoneName) const;
        [[nodiscard]] bool has(const std::string& humanBoneName) const;
        [[nodiscard]] std::vector<std::string> boneNames() const;
        [[nodiscard]] std::size_t size() const { return rawBones_.size(); }

        // --- normalized rig (Phase1): アニメーション(VRMA)適用のための正規化スケルトン ---
        // 各 human bone に対応する正規化ノード(rest world 回転=identity)を持つ。
        // 正規化ノードの local 回転にポーズを与え、updateRig() で raw ボーンへ転写する。
        [[nodiscard]] std::shared_ptr<threepp::Object3D> getNormalizedBoneNode(const std::string& name) const;
        void resetNormalizedPose();///< 正規化ノードを rest(identity) に戻す
        void updateRig();          ///< 正規化ポーズを raw ボーンへ転写 (hips は位置も転写)
        [[nodiscard]] bool hasRig() const { return static_cast<bool>(rigRoot_); }

        // --- hips ルートモーション用 ---
        [[nodiscard]] bool hasHips() const { return hasHips_; }
        [[nodiscard]] float hipsRestWorldY() const { return hipsRestWorldY_; }///< raw hips の rest world Y
        [[nodiscard]] threepp::Vector3 hipsRestNormPos() const { return hipsRestNormPos_; }///< 正規化 hips の rest 位置

    private:
        std::unordered_map<std::string, std::shared_ptr<threepp::Bone>> rawBones_;

        struct RigBone {
            std::shared_ptr<threepp::Bone> raw;
            std::shared_ptr<threepp::Object3D> norm;
            threepp::Quaternion rawRestWorldRot;///< raw ボーンの rest world 回転
            std::string parent;                 ///< 親 human bone 名 ("" ならルート)
        };
        std::unordered_map<std::string, RigBone> rig_;
        std::vector<std::string> rigOrder_;///< 親→子の順
        std::shared_ptr<threepp::Group> rigRoot_;
        bool hasHips_ = false;
        threepp::Vector3 hipsRestNormPos_;///< buildRig 時の正規化 hips 位置 (rest)
        float hipsRestWorldY_ = 0.f;      ///< raw hips の rest world Y (VRMA 高さスケール用)

        void buildRig();///< rawBones_ 確定後・scene の world 行列更新後に呼ぶ
        friend class VRMLoader;
    };

    /// 表情マネージャ (Phase1)。VRM の expression/blendShape を morph target 影響度へ反映する。
    /// Phase1 は morph bind のみ (material color / texture transform bind は Phase2/MToon 後)。
    class VRMExpressionManager {
    public:
        /// 表情ウェイトを設定 (0..1)。実際の反映は VRM::update()。
        void setValue(const std::string& name, float weight);
        [[nodiscard]] float getValue(const std::string& name) const;
        [[nodiscard]] bool has(const std::string& name) const;
        [[nodiscard]] std::vector<std::string> names() const;
        [[nodiscard]] std::size_t size() const { return expressions_.size(); }

        /// 全表情ウェイトを morph target 影響度へ適用 (bind 対象を 0 クリアしてから加算)。
        /// override(block/blend) を考慮し、blink/mouth/lookAt グループの表情を抑制する。
        void applyToScene();

        /// lookAt override による抑制係数 (0..1)。VRMLookAt が視線適用量に掛ける。
        [[nodiscard]] float lookAtMultiplier() const { return lookAtMul_; }

    private:
        enum class Override { None, Block, Blend };
        struct MorphBind {
            std::shared_ptr<threepp::Mesh> mesh;
            int index;    ///< morph target index
            float weight; ///< この表情が全開(1.0)のときの目標ウェイト
        };
        struct Expression {
            std::vector<MorphBind> morphBinds;
            bool isBinary = false;///< true なら 0.5 閾値で 0/1
            Override overrideBlink = Override::None;
            Override overrideLookAt = Override::None;
            Override overrideMouth = Override::None;
        };
        float lookAtMul_ = 1.f;///< applyToScene が更新する lookAt 抑制係数
        std::unordered_map<std::string, Expression> expressions_;
        std::unordered_map<std::string, float> values_;
        friend class VRMLoader;
    };

    /// 視線 (Phase1)。bone 方式のみ (leftEye/rightEye を回転)。expression 方式は今後。
    /// target(ワールド座標) を設定すると update() で頭からの方向を eye ボーンに反映する。
    class VRMLookAt {
    public:
        void setTarget(float x, float y, float z);///< ワールド座標の注視点
        void clearTarget();                       ///< 追従解除し eye を rest に戻す
        [[nodiscard]] bool hasTarget() const { return hasTarget_; }
        [[nodiscard]] bool valid() const { return static_cast<bool>(head_) && (leftEye_ || rightEye_); }
        void update();

    private:
        std::shared_ptr<threepp::Bone> head_, leftEye_, rightEye_;
        threepp::Vector3 offset_{0.f, 0.f, 0.f};   ///< 頭ボーンからの目位置オフセット
        threepp::Quaternion leftRest_, rightRest_; ///< eye ボーンの rest 回転
        float inMaxH_ = 90.f, inMaxV_ = 90.f;      ///< 入力角の最大 (deg)
        float outH_ = 10.f, outVUp_ = 10.f, outVDown_ = 10.f;///< 出力スケール (deg)
        threepp::Vector3 target_{0.f, 0.f, 0.f};
        bool hasTarget_ = false;
        friend class VRMLoader;
    };

    class VRMAnimation;// 前方宣言 (定義は下方)

    // === VRMC_springBone (揺れもの物理: 髪/服/しっぽ等) ==========================
    /// コライダー (球 or カプセル)。node のワールド行列 + offset に配置される。
    struct VRMSpringCollider {
        std::shared_ptr<threepp::Object3D> node;
        threepp::Vector3 offset{0.f, 0.f, 0.f};
        float radius = 0.f;
        bool capsule = false;
        threepp::Vector3 tail{0.f, 0.f, 0.f};///< capsule の他端 offset
    };

    /// spring joint (揺れボーン1個)。three-vrm の物理に準拠。
    struct VRMSpringJoint {
        std::shared_ptr<threepp::Object3D> bone;
        std::shared_ptr<threepp::Object3D> child;///< 無ければ nullptr (仮想 tail=7cm)
        std::shared_ptr<threepp::Object3D> center;///< center 空間の基準ノード (nullptr=world)
        float hitRadius = 0.f, stiffness = 1.f, gravityPower = 0.f, dragForce = 0.4f;
        threepp::Vector3 gravityDir{0.f, -1.f, 0.f};
        std::vector<int> colliderGroupIndices;///< この joint が衝突判定するグループ

        // 状態 (setInitState で初期化)
        threepp::Vector3 currentTail, prevTail, boneAxis, initialLocalChildPosition;
        threepp::Quaternion initialLocalRotation;
        threepp::Matrix4 initialLocalMatrix;
        float worldSpaceBoneLength = 0.f;
    };

    /// spring bone マネージャ。joints はチェーン順(親→子)に格納する。
    class VRMSpringBoneManager {
    public:
        std::vector<VRMSpringCollider> colliders;
        std::vector<std::vector<int>> colliderGroups;///< group -> collider index list
        std::vector<VRMSpringJoint> joints;
        /// コライダー半径の一律スケール (既定 1.0=忠実)。VRoid 系はコライダーが体にタイトで
        /// 胴体側面が手薄なため、>1 にすると髪が服に埋まりにくくなる (opt-in)。
        float colliderScale = 1.0f;

        [[nodiscard]] bool empty() const { return joints.empty(); }
        void setInitState();///< rest ポーズ(world 行列更新後)で捕捉
        void update(float dt);
    private:
        void updateJoint(VRMSpringJoint& j, float dt);
        void collide(const VRMSpringJoint& j, threepp::Vector3& tail, const threepp::Vector3& head);
    };

    /// VRMC_node_constraint (roll / aim / rotation)。あるノードが別ノード(source)の
    /// 回転や向きを追従する。ツイスト/ロールボーン等に使われる。humanoid rig 転写後・
    /// 描画前に update() で適用する。式は three-vrm 実装に準拠。
    struct VRMNodeConstraint {
        enum class Type { Roll, Aim, Rotation };
        Type type = Type::Roll;
        std::shared_ptr<threepp::Object3D> dst;///< 制約を受けるノード
        std::shared_ptr<threepp::Object3D> src;///< 参照元ノード
        float weight = 1.0f;
        threepp::Vector3 axis{1.f, 0.f, 0.f};  ///< roll軸 / aim軸 (単位)

        // rest 状態 (setInitState で rest ポーズ時に捕捉)
        threepp::Quaternion dstRest;
        threepp::Quaternion invDstRest;
        threepp::Quaternion invSrcRest;
        threepp::Quaternion invSrcRestMulDstRest;

        void setInitState();///< rest ポーズ(ロード直後)で呼ぶ
        void update();      ///< 毎フレーム (src が更新済みの状態で呼ぶ)
    };

    /// VRM モデル 1 体。scene + humanoid(raw) + meta + expressionManager + lookAt。
    /// Phase2+ で springBoneManager を追加。
    class VRM {
    public:
        std::shared_ptr<threepp::Group> scene;///< GLTFResult.scene をそのまま。Scene に add して描画する
        VRMHumanoid humanoid;
        VRMExpressionManager expressionManager;
        VRMLookAt lookAt;
        VRMMeta meta;
        std::vector<VRMNodeConstraint> constraints;///< VRMC_node_constraint (dependency 順)
        VRMSpringBoneManager springBone;           ///< VRMC_springBone (揺れもの)
        /// MToon ShaderMaterial が uniform で raw Texture* 参照するテクスチャの寿命保持。
        /// (元材差し替えで解放されないよう VRM が所有し続ける)
        std::vector<std::shared_ptr<threepp::Texture>> keepAliveTextures;

        /// MToon UV アニメーション (スクロール/回転)。update(dt) で uvOffset/uvRotPhase を更新。
        struct UVAnim {
            std::shared_ptr<threepp::ShaderMaterial> material;
            float scrollX = 0.f, scrollY = 0.f, rotSpeed = 0.f;
        };
        std::vector<UVAnim> uvAnims;
        float uvAnimTime = 0.f;

        /// 毎フレーム更新 (ポーズ + 制約 + 表情 + 視線 + springBone)。
        /// deltaSec は springBone 物理積分に使う (0 だと揺れは止まる)。
        void update(float deltaSec = 0.0f);

        /// VRMC_node_constraint を全て適用 (humanoid rig 転写後・描画前)。
        void updateConstraints();

        /// springBone コライダー半径の一律スケール (既定 1.0)。VRoid 系で胴体側面が手薄で
        /// 髪が服に埋まる場合、1.2〜1.4 程度にすると改善する (opt-in、標準外)。
        void setColliderScale(float s) { springBone.colliderScale = s; }
        [[nodiscard]] float getColliderScale() const { return springBone.colliderScale; }

        /// VRM アニメーションを時刻 time(sec) で正規化ボーンへ適用する。
        /// 適用後 update() を呼ぶと raw へ転写される (update 内で updateRig)。
        void applyAnimation(const VRMAnimation& anim, float time);
    };

    /// VRM ローダ。threepp GLTFLoader で読み、生 glTF JSON の VRMC_vrm 拡張を解釈して VRM を組む。
    class VRMLoader {
    public:
        /// 失敗時、または VRMC_vrm 拡張が無い場合は nullptr を返す (VRM 0.x は Phase3)。
        std::shared_ptr<VRM> load(const std::filesystem::path& path);
        /// メモリ上の VRM(GLB) バイト列から読む。吉里吉里ストリーム/アーカイブ経由用。
        std::shared_ptr<VRM> loadFromMemory(const unsigned char* data, std::size_t size);

    private:
        /// GLTFResult を VRM として解釈する共通処理 (load / loadFromMemory 兼用)。
        std::shared_ptr<VRM> buildVRM(std::optional<threepp::GLTFResult>& result,
                                      const std::string& srcLabel);
    };

    /// VRM アニメーション (.vrma / VRMC_vrm_animation)。モデル非依存のボーン回転トラック集合。
    /// Phase1 はヒューマノイドボーンの rotation のみ (hips translation / 表情 / 視線トラックは今後)。
    class VRMAnimation {
    public:
        [[nodiscard]] float duration() const { return duration_; }
        [[nodiscard]] std::size_t boneTrackCount() const { return boneRotations_.size(); }
        [[nodiscard]] bool has(const std::string& boneName) const { return boneRotations_.count(boneName) > 0; }

        [[nodiscard]] bool hasHipsTranslation() const { return hasHips_; }
        [[nodiscard]] std::size_t expressionTrackCount() const { return expressionTracks_.size(); }

    private:
        struct RotTrack {
            std::vector<float> times; ///< キー時刻 (sec)
            std::vector<float> values;///< クォータニオン xyzw を各キー分 (times.size()*4)
        };
        struct VecTrack {
            std::vector<float> times; ///< キー時刻 (sec)
            std::vector<float> values;///< xyz を各キー分 (times.size()*3)
        };
        struct ScalarTrack {
            std::vector<float> times;
            std::vector<float> values;///< 1 キー 1 値
        };
        std::unordered_map<std::string, RotTrack> boneRotations_;
        // hips ルートモーション: 正規化空間での hips 位置トラック (VRMA ローカル)
        bool hasHips_ = false;
        VecTrack hipsTranslation_;
        float vrmaHipsRestY_ = 0.f;      ///< VRMA の hips rest world Y (高さスケール用)
        threepp::Vector3 vrmaHipsRest_;  ///< VRMA hips ノードの rest ローカル位置 (offset 基準)
        // 表情トラック: 表情名 -> ウェイト時系列 (VRMA node.translation.x を採用)
        std::unordered_map<std::string, ScalarTrack> expressionTracks_;
        float duration_ = 0.f;
        friend class VRMAnimationLoader;
        friend class VRM;
    };

    class VRMAnimationLoader {
    public:
        /// .vrma を読み込む。VRMC_vrm_animation が無ければ nullptr。
        std::shared_ptr<VRMAnimation> load(const std::filesystem::path& path);
        /// メモリ上の .vrma(GLB) バイト列から読む。吉里吉里ストリーム/アーカイブ経由用。
        std::shared_ptr<VRMAnimation> loadFromMemory(const unsigned char* data, std::size_t size);

    private:
        std::shared_ptr<VRMAnimation> buildVRMA(std::optional<threepp::GLTFResult>& result,
                                                const std::string& srcLabel);
    };

}// namespace krkrvrm

#endif// KRKR_THREEPP_VRM_HPP
