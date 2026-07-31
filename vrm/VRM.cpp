#include "VRM.hpp"

#include <threepp/loaders/GLTFLoader.hpp>

// VRM.h は nlohmann::json を前提にするので先に include する。
#include <nlohmann/json.hpp>

#ifndef USE_VRMC_VRM_1_0
#define USE_VRMC_VRM_1_0// 通常は CMake の DEFINITIONS で定義済み
#endif
#include <VRMC/VRM.h>

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

    // -----------------------------------------------------------------------
    // VRMLoader
    // -----------------------------------------------------------------------
    std::shared_ptr<VRM> VRMLoader::load(const std::filesystem::path& path) {
        threepp::GLTFLoader loader;
        auto result = loader.load(path);
        if (!result) {
            std::cerr << "[VRM] GLTF load failed: " << path << "\n";
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
            std::cerr << "[VRM] no VRM/VRMC_vrm extension (not a VRM file): " << path << "\n";
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
        }

        std::cerr << "[VRM] loaded (" << (isVrm1 ? "1.0" : "0.x") << "): bones="
                  << vrm->humanoid.size() << " meta.name=\"" << vrm->meta.name << "\"\n";
        return vrm;
    }

}// namespace krkrvrm
