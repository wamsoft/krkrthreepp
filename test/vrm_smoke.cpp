// VRM Phase 0 スタンドアロン検証 (GLES/kirikiri 不要)。
// threepp GLTFLoader 露出改造 + infosia/VRM.h + associations が実 VRM から
// ボーン/メタ/シーンを取り出せることを確認する。
//
//   vrm_smoke <path-to.vrm>
//
// build: cmake --preset x64-windows -DBUILD_VRM_SMOKE_TEST=ON && cmake --build ...

#include "VRM.hpp"

#include <threepp/core/Object3D.hpp>
#include <threepp/math/Euler.hpp>

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

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

    // メモリロード経路 (ホストのストリーム/アーカイブ用) も同じ結果になるか検証。
    {
        std::ifstream f(path, std::ios::binary);
        std::vector<unsigned char> bytes(std::istreambuf_iterator<char>(f), {});
        krkrvrm::VRMLoader memLoader;
        auto vrmMem = memLoader.loadFromMemory(bytes.data(), bytes.size());
        if (!vrmMem) {
            std::cout << "[smoke] FAILED: loadFromMemory returned null\n";
            return 1;
        }
        if (vrmMem->humanoid.size() != vrm->humanoid.size()) {
            std::cout << "[smoke] FAILED: memory bones=" << vrmMem->humanoid.size()
                      << " != file bones=" << vrm->humanoid.size() << "\n";
            return 1;
        }
        std::cout << "[smoke] loadFromMemory OK (bones match: " << vrmMem->humanoid.size() << ")\n";
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

    // --- Expression (Phase1) ---
    auto exprNames = vrm->expressionManager.names();
    std::cout << "  expressions      = " << exprNames.size() << "\n";
    bool exprApplied = false;
    if (!exprNames.empty()) {
        // まばたき/母音系のどれかを選ぶ (版で名前が違う: 0.x=a/blink, 1.0=aa/blink)
        std::string pick;
        for (const char* cand : {"aa", "a", "happy", "blink", "e", "ee"}) {
            if (vrm->expressionManager.has(cand)) { pick = cand; break; }
        }
        if (pick.empty()) pick = exprNames.front();

        // 適用前の bound morph 合計を測る
        auto sumInfluences = [&]() {
            double s = 0;
            vrm->scene->traverse([&](threepp::Object3D& o) {
                if (auto* m = dynamic_cast<threepp::Mesh*>(&o))
                    for (float w : m->morphTargetInfluences()) s += w;
            });
            return s;
        };
        vrm->expressionManager.setValue(pick, 0.f);
        vrm->update();
        const double before = sumInfluences();
        vrm->expressionManager.setValue(pick, 1.f);
        vrm->update();
        const double after = sumInfluences();
        std::cout << "  expr \"" << pick << "\": morph sum " << before << " -> " << after << "\n";
        exprApplied = (after > before + 1e-4);
        std::cout << "[smoke] expression drives morph: " << (exprApplied ? "yes" : "no") << "\n";
    }

    // --- LookAt (Phase1, bone 方式) ---
    bool lookAtOk = true;// eye ボーンが無いモデルは対象外 (true 扱い)
    auto leftEye = vrm->humanoid.getRawBoneNode("leftEye");
    if (leftEye) {
        const threepp::Quaternion before = leftEye->quaternion;
        vrm->lookAt.setTarget(2.0f, 1.0f, 2.0f);// 頭の右前あたり
        vrm->update();
        const threepp::Quaternion after = leftEye->quaternion;
        const float dq = std::abs(before.x - after.x) + std::abs(before.y - after.y) +
                         std::abs(before.z - after.z) + std::abs(before.w - after.w);
        lookAtOk = (dq > 1e-5f);
        std::cout << "  lookAt: leftEye quaternion delta = " << dq
                  << " -> " << (lookAtOk ? "moved" : "unchanged") << "\n";
        vrm->lookAt.clearTarget();
    } else {
        std::cout << "  lookAt: no eye bones (skip)\n";
    }

    // --- Normalized rig (Phase1) ---
    bool rigOk = true;
    if (vrm->humanoid.hasRig()) {
        auto testBone = vrm->humanoid.getRawBoneNode("leftUpperArm");
        if (!testBone) testBone = vrm->humanoid.getRawBoneNode("spine");

        auto qdelta = [](const threepp::Quaternion& a, const threepp::Quaternion& b) {
            return std::abs(a.x - b.x) + std::abs(a.y - b.y) + std::abs(a.z - b.z) + std::abs(a.w - b.w);
        };

        // 恒等ポーズ往復: 正規化を rest にして updateRig → raw が rest のまま(数学的に保証)
        const threepp::Quaternion rest = testBone ? testBone->quaternion : threepp::Quaternion();
        vrm->humanoid.resetNormalizedPose();
        vrm->humanoid.updateRig();
        const double dRest = testBone ? qdelta(testBone->quaternion, rest) : 0.0;

        // 正規化ボーンを回すと raw が動く
        double dMove = 0.0;
        auto normArm = vrm->humanoid.getNormalizedBoneNode("leftUpperArm");
        if (normArm && testBone) {
            const threepp::Quaternion before = testBone->quaternion;
            normArm->quaternion.setFromEuler(threepp::Euler(0.f, 0.f, 0.5f));
            vrm->humanoid.updateRig();
            dMove = qdelta(testBone->quaternion, before);
            vrm->humanoid.resetNormalizedPose();
            vrm->humanoid.updateRig();
        }
        rigOk = (dRest < 1e-3) && (!(normArm && testBone) || dMove > 1e-4);
        std::cout << "  rig: bones=" << (vrm->humanoid.hasRig() ? "yes" : "no")
                  << " identity-roundtrip=" << dRest << " (want~0)"
                  << " move=" << dMove << " (want>0) -> " << (rigOk ? "ok" : "FAIL") << "\n";
    }

    // --- VRMA (Phase1): 第2引数に .vrma を渡すと適用検証 ---
    bool vrmaOk = true;
    if (argc >= 3) {
        krkrvrm::VRMAnimationLoader aloader;
        auto anim = aloader.load(argv[2]);
        if (!anim) {
            std::cout << "  vrma: load failed\n";
            vrmaOk = false;
        } else {
            std::cout << "  vrma: boneTracks=" << anim->boneTrackCount()
                      << " duration=" << anim->duration() << "\n";
            auto tb = vrm->humanoid.getRawBoneNode("leftUpperArm");
            const threepp::Quaternion before = tb ? tb->quaternion : threepp::Quaternion();
            vrm->applyAnimation(*anim, anim->duration() * 0.5f);
            vrm->update();// updateRig で raw へ転写
            double d = 0.0;
            if (tb) {
                const auto& q = tb->quaternion;
                d = std::abs(q.x - before.x) + std::abs(q.y - before.y) +
                    std::abs(q.z - before.z) + std::abs(q.w - before.w);
            }
            std::cout << "  vrma applied @half: leftUpperArm raw delta=" << d << "\n";
            vrmaOk = (anim->boneTrackCount() > 0) && (!tb || d > 1e-4);
        }
    }

    const bool ok = vrm->scene && vrm->humanoid.has("hips") && found >= 5 &&
                    (exprNames.empty() || exprApplied) && lookAtOk && rigOk && vrmaOk;
    std::cout << "[smoke] RESULT: " << (ok ? "PASS" : "FAIL") << "\n";
    return ok ? 0 : 1;
}
