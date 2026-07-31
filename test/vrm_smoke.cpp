// VRM Phase 0 スタンドアロン検証 (GLES/kirikiri 不要)。
// threepp GLTFLoader 露出改造 + infosia/VRM.h + associations が実 VRM から
// ボーン/メタ/シーンを取り出せることを確認する。
//
//   vrm_smoke <path-to.vrm>
//
// build: cmake --preset x64-windows -DBUILD_VRM_SMOKE_TEST=ON && cmake --build ...

#include "VRM.hpp"

#include <threepp/core/Object3D.hpp>

#include <iostream>
#include <string>

static int countNodes(const std::shared_ptr<threepp::Object3D>& root) {
    if (!root) return 0;
    int n = 0;
    root->traverse([&](threepp::Object3D&) { ++n; });
    return n;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: vrm_smoke <path-to.vrm>\n";
        return 2;
    }
    const std::string path = argv[1];
    std::cout << "[smoke] loading: " << path << "\n";

    krkrvrm::VRMLoader loader;
    auto vrm = loader.load(path);
    if (!vrm) {
        std::cout << "[smoke] FAILED: loader returned null\n";
        return 1;
    }

    std::cout << "[smoke] OK\n";
    std::cout << "  meta.name        = \"" << vrm->meta.name << "\"\n";
    std::cout << "  meta.version     = \"" << vrm->meta.version << "\"\n";
    std::cout << "  meta.licenseUrl  = \"" << vrm->meta.licenseUrl << "\"\n";
    std::cout << "  meta.authors     = " << vrm->meta.authors.size() << " entry(ies)";
    if (!vrm->meta.authors.empty()) std::cout << " (first=\"" << vrm->meta.authors.front() << "\")";
    std::cout << "\n";
    std::cout << "  humanoid bones   = " << vrm->humanoid.size() << "\n";
    std::cout << "  scene nodes      = " << countNodes(vrm->scene) << "\n";

    // 主要ボーンの有無 (最低限これらが取れれば humanoid 解決は成功)
    const char* key[] = {"hips", "spine", "chest", "neck", "head",
                         "leftUpperArm", "rightUpperArm", "leftUpperLeg", "rightUpperLeg"};
    std::cout << "  key bones:\n";
    int found = 0;
    for (const char* k : key) {
        auto b = vrm->humanoid.getRawBoneNode(k);
        std::cout << "    " << k << " -> " << (b ? ("\"" + b->name + "\"") : std::string("(missing)")) << "\n";
        if (b) ++found;
    }
    std::cout << "[smoke] key bones found: " << found << "/" << (sizeof(key) / sizeof(key[0])) << "\n";

    const bool ok = vrm->scene && vrm->humanoid.has("hips") && found >= 5;
    std::cout << "[smoke] RESULT: " << (ok ? "PASS" : "FAIL") << "\n";
    return ok ? 0 : 1;
}
