#ifndef KRKR_THREEPP_VRM_HPP
#define KRKR_THREEPP_VRM_HPP

// Phase 0 VRM runtime (pure C++, no ncbind).
// See docs/vrm_phase0_design.md. Depends on threepp + infosia/VRM.h + nlohmann.

#include <threepp/objects/Bone.hpp>
#include <threepp/objects/Group.hpp>
#include <threepp/objects/Mesh.hpp>
#include <threepp/math/Quaternion.hpp>
#include <threepp/math/Vector3.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

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
        void updateRig();          ///< 正規化ポーズを raw ボーンへ転写
        [[nodiscard]] bool hasRig() const { return static_cast<bool>(rigRoot_); }

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
        void applyToScene();

    private:
        struct MorphBind {
            std::shared_ptr<threepp::Mesh> mesh;
            int index;    ///< morph target index
            float weight; ///< この表情が全開(1.0)のときの目標ウェイト
        };
        struct Expression {
            std::vector<MorphBind> morphBinds;
            bool isBinary = false;///< true なら 0.5 閾値で 0/1
        };
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

    /// VRM モデル 1 体。scene + humanoid(raw) + meta + expressionManager + lookAt。
    /// Phase2+ で springBoneManager を追加。
    class VRM {
    public:
        std::shared_ptr<threepp::Group> scene;///< GLTFResult.scene をそのまま。Scene に add して描画する
        VRMHumanoid humanoid;
        VRMExpressionManager expressionManager;
        VRMLookAt lookAt;
        VRMMeta meta;

        /// 毎フレーム更新 (Phase1: ポーズ + 表情 + 視線。Phase2 で springBone を追加)。
        void update(float deltaSec = 0.0f);

        /// VRM アニメーションを時刻 time(sec) で正規化ボーンへ適用する。
        /// 適用後 update() を呼ぶと raw へ転写される (update 内で updateRig)。
        void applyAnimation(const VRMAnimation& anim, float time);
    };

    /// VRM ローダ。threepp GLTFLoader で読み、生 glTF JSON の VRMC_vrm 拡張を解釈して VRM を組む。
    class VRMLoader {
    public:
        /// 失敗時、または VRMC_vrm 拡張が無い場合は nullptr を返す (VRM 0.x は Phase3)。
        std::shared_ptr<VRM> load(const std::filesystem::path& path);
    };

    /// VRM アニメーション (.vrma / VRMC_vrm_animation)。モデル非依存のボーン回転トラック集合。
    /// Phase1 はヒューマノイドボーンの rotation のみ (hips translation / 表情 / 視線トラックは今後)。
    class VRMAnimation {
    public:
        [[nodiscard]] float duration() const { return duration_; }
        [[nodiscard]] std::size_t boneTrackCount() const { return boneRotations_.size(); }
        [[nodiscard]] bool has(const std::string& boneName) const { return boneRotations_.count(boneName) > 0; }

    private:
        struct RotTrack {
            std::vector<float> times; ///< キー時刻 (sec)
            std::vector<float> values;///< クォータニオン xyzw を各キー分 (times.size()*4)
        };
        std::unordered_map<std::string, RotTrack> boneRotations_;
        float duration_ = 0.f;
        friend class VRMAnimationLoader;
        friend class VRM;
    };

    class VRMAnimationLoader {
    public:
        /// .vrma を読み込む。VRMC_vrm_animation が無ければ nullptr。
        std::shared_ptr<VRMAnimation> load(const std::filesystem::path& path);
    };

}// namespace krkrvrm

#endif// KRKR_THREEPP_VRM_HPP
