#ifndef KRKR_THREEPP_VRM_HPP
#define KRKR_THREEPP_VRM_HPP

// Phase 0 VRM runtime (pure C++, no ncbind).
// See docs/vrm_phase0_design.md. Depends on threepp + infosia/VRM.h + nlohmann.

#include <threepp/objects/Bone.hpp>
#include <threepp/objects/Group.hpp>

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

    private:
        std::unordered_map<std::string, std::shared_ptr<threepp::Bone>> rawBones_;
        friend class VRMLoader;
    };

    /// VRM モデル 1 体。Phase0 は表示に必要な scene + humanoid(raw) + meta。
    /// Phase1+ で expressionManager / lookAt / springBoneManager / update(dt) を追加。
    class VRM {
    public:
        std::shared_ptr<threepp::Group> scene;///< GLTFResult.scene をそのまま。Scene に add して描画する
        VRMHumanoid humanoid;
        VRMMeta meta;
    };

    /// VRM ローダ。threepp GLTFLoader で読み、生 glTF JSON の VRMC_vrm 拡張を解釈して VRM を組む。
    class VRMLoader {
    public:
        /// 失敗時、または VRMC_vrm 拡張が無い場合は nullptr を返す (VRM 0.x は Phase3)。
        std::shared_ptr<VRM> load(const std::filesystem::path& path);
    };

}// namespace krkrvrm

#endif// KRKR_THREEPP_VRM_HPP
