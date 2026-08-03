// Windowsのmin/maxマクロを無効化（threeppのstd::min/maxとの衝突を防ぐ）
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "ncbind.hpp"
#include "LayerExBase.hpp"
#include "vrm/VRM.hpp"
#include <vector>
#include <memory>
#include <cstdint>
#include <climits>

// threepp headers
#include <threepp/threepp.hpp>
#include <threepp/math/Vector2.hpp>
#include <threepp/math/Vector3.hpp>
#include <threepp/math/Vector4.hpp>
#include <threepp/math/Color.hpp>
#include <threepp/math/Quaternion.hpp>
#include <threepp/math/Euler.hpp>
#include <threepp/math/Matrix3.hpp>
#include <threepp/math/Matrix4.hpp>
#include <threepp/math/Box2.hpp>
#include <threepp/math/Box3.hpp>
#include <threepp/math/Sphere.hpp>
#include <threepp/math/Ray.hpp>
#include <threepp/math/Plane.hpp>
#include <threepp/math/Line3.hpp>
#include <threepp/math/Triangle.hpp>
#include <threepp/math/Frustum.hpp>
#include <threepp/math/Spherical.hpp>
#include <threepp/math/Cylindrical.hpp>

// Geometries
#include <threepp/geometries/CylinderGeometry.hpp>
#include <threepp/geometries/ConeGeometry.hpp>
#include <threepp/geometries/TorusGeometry.hpp>
#include <threepp/geometries/TorusKnotGeometry.hpp>
#include <threepp/geometries/CircleGeometry.hpp>
#include <threepp/geometries/RingGeometry.hpp>
#include <threepp/geometries/IcosahedronGeometry.hpp>
#include <threepp/geometries/OctahedronGeometry.hpp>
#include <threepp/geometries/CapsuleGeometry.hpp>
#include <threepp/geometries/LatheGeometry.hpp>
#include <threepp/geometries/EdgesGeometry.hpp>
#include <threepp/geometries/WireframeGeometry.hpp>

// Textures
#include <threepp/textures/Texture.hpp>
#include <threepp/textures/DataTexture.hpp>

// Loaders
#include <threepp/loaders/TextureLoader.hpp>
#include <threepp/loaders/GLTFLoader.hpp>

// Animation
#include <threepp/animation/AnimationMixer.hpp>

// Materials (additional)
#include <threepp/materials/ShaderMaterial.hpp>
#include <threepp/materials/RawShaderMaterial.hpp>
#include <threepp/materials/LineBasicMaterial.hpp>
#include <threepp/materials/PointsMaterial.hpp>
#include <threepp/materials/SpriteMaterial.hpp>
#include <threepp/materials/MeshNormalMaterial.hpp>
#include <threepp/materials/MeshDepthMaterial.hpp>
#include <threepp/materials/MeshToonMaterial.hpp>
#include <threepp/materials/MeshMatcapMaterial.hpp>

// Objects (additional)
#include <threepp/objects/Line.hpp>
#include <threepp/objects/LineSegments.hpp>
#include <threepp/objects/Points.hpp>
#include <threepp/objects/Sprite.hpp>
#include <threepp/objects/InstancedMesh.hpp>
#include <threepp/objects/LOD.hpp>
#include <threepp/objects/Bone.hpp>
#include <threepp/objects/Skeleton.hpp>
#include <threepp/objects/SkinnedMesh.hpp>
#include <threepp/objects/Sky.hpp>
#include <threepp/objects/Water.hpp>
#include <threepp/objects/Reflector.hpp>

// Curves
#include <threepp/extras/core/Curve.hpp>
#include <threepp/extras/core/CurvePath.hpp>
#include <threepp/extras/core/Path.hpp>
#include <threepp/extras/core/Shape.hpp>
#include <threepp/extras/curves/CatmullRomCurve3.hpp>

// Shape-based Geometries
#include <threepp/geometries/ShapeGeometry.hpp>
#include <threepp/geometries/ExtrudeGeometry.hpp>
#include <threepp/extras/curves/LineCurve.hpp>
#include <threepp/extras/curves/QuadraticBezierCurve.hpp>
#include <threepp/extras/curves/QuadraticBezierCurve3.hpp>
#include <threepp/extras/curves/CubicBezierCurve.hpp>
#include <threepp/extras/curves/CubicBezierCurve3.hpp>
#include <threepp/extras/curves/SplineCurve.hpp>
#include <threepp/extras/curves/EllipseCurve.hpp>

// Helpers
#include <threepp/helpers/AxesHelper.hpp>
#include <threepp/helpers/GridHelper.hpp>
#include <threepp/helpers/ArrowHelper.hpp>
#include <threepp/helpers/BoxHelper.hpp>
#include <threepp/helpers/CameraHelper.hpp>
#include <threepp/helpers/DirectionalLightHelper.hpp>
#include <threepp/helpers/PointLightHelper.hpp>
#include <threepp/helpers/SpotLightHelper.hpp>
#include <threepp/helpers/PolarGridHelper.hpp>
#include <threepp/helpers/PlaneHelper.hpp>

// Scenes
#include <threepp/scenes/Fog.hpp>
#include <threepp/scenes/FogExp2.hpp>

// Core
#include <threepp/core/Raycaster.hpp>

using namespace threepp;

/**
 * ログ出力用
 */
void
message_log(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	char msg[1024];
	_vsnprintf_s(msg, 1024, _TRUNCATE, format, args);
	TVPAddLog(ttstr(msg));
	va_end(args);
}

/**
 * エラーログ出力用
 */
void
error_log(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	char msg[1024];
	_vsnprintf_s(msg, 1024, _TRUNCATE, format, args);
	TVPAddImportantLog(ttstr(msg));
	va_end(args);
}

static void initThreepp()
{
	// 本家 threepp は three.js 由来のカラーマネジメント(sRGB↔linear)を
	// デフォルト有効にしているが、従来の描画結果を維持するため無効化する。
	ColorManagement::enabled = false;
}

static void deInitThreepp()
{
}

// ----------------------------------------------------------------
// 実体型の登録
// 数値パラメータ系は配列か辞書を使えるような特殊コンバータを構築
// ----------------------------------------------------------------

// 両方自前コンバータ
#define NCB_SET_CONVERTOR_BOTH(type, convertor)\
NCB_TYPECONV_SRCMAP_SET(type, convertor<type>, true);\
NCB_TYPECONV_DSTMAP_SET(type, convertor<type>, true)

// SRCだけ自前コンバータ
#define NCB_SET_CONVERTOR_SRC(type, convertor)\
NCB_TYPECONV_SRCMAP_SET(type, convertor<type>, true);\
NCB_TYPECONV_DSTMAP_SET(type, ncbNativeObjectBoxing::Unboxing, true)

// DSTだけ自前コンバータ
#define NCB_SET_CONVERTOR_DST(type, convertor)\
NCB_TYPECONV_SRCMAP_SET(type, ncbNativeObjectBoxing::Boxing,   true); \
NCB_TYPECONV_DSTMAP_SET(type, convertor<type>, true)

/**
 * 配列かどうかの判定
 * @param var VARIANT
 * @return 配列なら true
 */
bool IsArray(const tTJSVariant &var)
{
	if (var.Type() == tvtObject) {
		iTJSDispatch2 *obj = var.AsObjectNoAddRef();
		return obj->IsInstanceOf(0, NULL, NULL, TJS_W("Array"), obj) == TJS_S_TRUE;
	}
	return false;
}

// T& 戻り値メソッドを void に変換するプロキシマクロ（値型クラス用）
// NCB_METHOD_VOID0(name) - 引数なしメソッド
// NCB_METHOD_VOID1(name, T1) - 引数1つ
// etc.
#define NCB_METHOD_VOID0(name) \
	struct VoidProxy_ ## name { static void Proxy(Class *inst) { inst->name(); } }; \
	NCB_METHOD_PROXY(name, VoidProxy_ ## name::Proxy)

#define NCB_METHOD_VOID1(name, T1) \
	struct VoidProxy_ ## name { static void Proxy(Class *inst, T1 a1) { inst->name(a1); } }; \
	NCB_METHOD_PROXY(name, VoidProxy_ ## name::Proxy)

#define NCB_METHOD_VOID2(name, T1, T2) \
	struct VoidProxy_ ## name { static void Proxy(Class *inst, T1 a1, T2 a2) { inst->name(a1, a2); } }; \
	NCB_METHOD_PROXY(name, VoidProxy_ ## name::Proxy)

#define NCB_METHOD_VOID3(name, T1, T2, T3) \
	struct VoidProxy_ ## name { static void Proxy(Class *inst, T1 a1, T2 a2, T3 a3) { inst->name(a1, a2, a3); } }; \
	NCB_METHOD_PROXY(name, VoidProxy_ ## name::Proxy)

#define NCB_METHOD_VOID4(name, T1, T2, T3, T4) \
	struct VoidProxy_ ## name { static void Proxy(Class *inst, T1 a1, T2 a2, T3 a3, T4 a4) { inst->name(a1, a2, a3, a4); } }; \
	NCB_METHOD_PROXY(name, VoidProxy_ ## name::Proxy)

#define NCB_METHOD_VOID5(name, T1, T2, T3, T4, T5) \
	struct VoidProxy_ ## name { static void Proxy(Class *inst, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5) { inst->name(a1, a2, a3, a4, a5); } }; \
	NCB_METHOD_PROXY(name, VoidProxy_ ## name::Proxy)

#define NCB_METHOD_VOID6(name, T1, T2, T3, T4, T5, T6) \
	struct VoidProxy_ ## name { static void Proxy(Class *inst, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5, T6 a6) { inst->name(a1, a2, a3, a4, a5, a6); } }; \
	NCB_METHOD_PROXY(name, VoidProxy_ ## name::Proxy)

#define NCB_METHOD_VOID9(name, T1, T2, T3, T4, T5, T6, T7, T8, T9) \
	struct VoidProxy_ ## name { static void Proxy(Class *inst, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5, T6 a6, T7 a7, T8 a8, T9 a9) { inst->name(a1, a2, a3, a4, a5, a6, a7, a8, a9); } }; \
	NCB_METHOD_PROXY(name, VoidProxy_ ## name::Proxy)

#define NCB_METHOD_VOID16(name, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16) \
	struct VoidProxy_ ## name { static void Proxy(Class *inst, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5, T6 a6, T7 a7, T8 a8, T9 a9, T10 a10, T11 a11, T12 a12, T13 a13, T14 a14, T15 a15, T16 a16) { inst->name(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16); } }; \
	NCB_METHOD_PROXY(name, VoidProxy_ ## name::Proxy)

// ThreeWrapper 用の T& 戻り値メソッド void 変換マクロ
#define NCB_THREE_METHOD_VOID0(name) \
	struct ThreeVoidProxy_ ## name { static void Proxy(ThreeWrapper<ThreeClass> *inst) { if (inst && inst->getObject()) inst->getObject()->name(); } }; \
	NCB_METHOD_PROXY(name, ThreeVoidProxy_ ## name::Proxy)

#define NCB_THREE_METHOD_VOID1(name, T1) \
	struct ThreeVoidProxy_ ## name { static void Proxy(ThreeWrapper<ThreeClass> *inst, T1 a1) { if (inst && inst->getObject()) inst->getObject()->name(a1); } }; \
	NCB_METHOD_PROXY(name, ThreeVoidProxy_ ## name::Proxy)

#define NCB_THREE_METHOD_VOID2(name, T1, T2) \
	struct ThreeVoidProxy_ ## name { static void Proxy(ThreeWrapper<ThreeClass> *inst, T1 a1, T2 a2) { if (inst && inst->getObject()) inst->getObject()->name(a1, a2); } }; \
	NCB_METHOD_PROXY(name, ThreeVoidProxy_ ## name::Proxy)

#define NCB_THREE_METHOD_VOID3(name, T1, T2, T3) \
	struct ThreeVoidProxy_ ## name { static void Proxy(ThreeWrapper<ThreeClass> *inst, T1 a1, T2 a2, T3 a3) { if (inst && inst->getObject()) inst->getObject()->name(a1, a2, a3); } }; \
	NCB_METHOD_PROXY(name, ThreeVoidProxy_ ## name::Proxy)

// メンバ変数をプロパティとして登録
#define NCB_MEMBER_PROPERTY(name, type, membername) \
	struct AutoProp_ ## name { \
		static void ProxySet(Class *inst, type value) { inst->membername = value; } \
		static type ProxyGet(Class *inst) {      return inst->membername; } }; \
	NCB_PROPERTY_PROXY(name,AutoProp_ ## name::ProxyGet, AutoProp_ ## name::ProxySet)

// 読み取り専用メンバプロパティ
#define NCB_MEMBER_PROPERTY_RO(name, type, membername) \
	struct AutoProp_ ## name { \
		static type ProxyGet(Class *inst) {      return inst->membername; } }; \
	Property(TJS_W(# name), &AutoProp_ ## name::ProxyGet, (int)0, Proxy)

// ポインタ引数型の getter を変換登録
#define NCB_ARG_PROPERTY_RO(name, type, methodname) \
	struct AutoProp_ ## name { \
		static type ProxyGet(Class *inst) { type var; inst->methodname(&var); return var; } }; \
	Property(TJS_W(# name), &AutoProp_ ## name::ProxyGet, (int)0, Proxy)

// ------------------------------------------------------
// 型コンバータ登録
// ------------------------------------------------------

// std::string ←→ tTJSVariant コンバータ
struct StdStringConvertor {
	void operator ()(tTJSVariant &dst, const std::string &src) {
		std::wstring ws(src.begin(), src.end());
		dst = ttstr(ws.c_str());
	}
	void operator ()(std::string &dst, const tTJSVariant &src) {
		if (src.Type() == tvtString) {
			const tjs_char *ws = src.GetString();
			std::string s;
			while (*ws) {
				if (*ws < 128) s += (char)*ws; else s += '?';
				ws++;
			}
			dst = s;
		} else {
			dst.clear();
		}
	}
};
NCB_SET_CONVERTOR(std::string, StdStringConvertor);

// size_t は 64bit では unsigned __int64 だが 32bit では unsigned int と同一。
// 32bit で登録すると ncbind 既定の unsigned int 変換と二重定義になる（C2766）。
// size_t が unsigned int より広い（=64bit）ときのみ登録する。
#if SIZE_MAX != UINT_MAX
NCB_TYPECONV_CAST_INTEGER(size_t);
#endif
NCB_TYPECONV_CAST_INTEGER(Side);
NCB_TYPECONV_CAST_INTEGER(Blending);
NCB_TYPECONV_CAST_INTEGER(BlendEquation);
NCB_TYPECONV_CAST_INTEGER(BlendFactor);
NCB_TYPECONV_CAST_INTEGER(DepthFunc);
NCB_TYPECONV_CAST_INTEGER(CombineOperation);
NCB_TYPECONV_CAST_INTEGER(ToneMapping);
NCB_TYPECONV_CAST_INTEGER(Mapping);
NCB_TYPECONV_CAST_INTEGER(TextureWrapping);
NCB_TYPECONV_CAST_INTEGER(Filter);
NCB_TYPECONV_CAST_INTEGER(Format);
NCB_TYPECONV_CAST_INTEGER(ColorSpace);
NCB_TYPECONV_CAST_INTEGER(CullFace);
NCB_TYPECONV_CAST_INTEGER(ShadowMap);
NCB_TYPECONV_CAST_INTEGER(Loop);
NCB_TYPECONV_CAST_INTEGER(Interpolation);
NCB_TYPECONV_CAST_INTEGER(NormalMapType);
NCB_TYPECONV_CAST_INTEGER(StencilOp);
NCB_TYPECONV_CAST_INTEGER(StencilFunc);
NCB_TYPECONV_CAST_INTEGER(DrawUsage);
NCB_TYPECONV_CAST_INTEGER(Euler::RotationOrders);

// ------------------------------------------------------- Vector2
template <class T>
struct Vector2Convertor {
	typedef ncbInstanceAdaptor<T> AdaptorT;
	template <typename ANYT>
	void operator ()(ANYT &adst, const tTJSVariant &src) {
		if (src.Type() == tvtObject) {
			T *obj = AdaptorT::GetNativeInstance(src.AsObjectNoAddRef());
			if (obj) {
				dst = *obj;
			} else {
				ncbPropAccessor info(src);
				if (IsArray(src)) {
					dst = Vector2((float)info.getRealValue(0),
								  (float)info.getRealValue(1));
				} else {
					dst = Vector2((float)info.getRealValue(TJS_W("x")),
								  (float)info.getRealValue(TJS_W("y")));
				}
			}
		} else {
			dst = T();
		}
		adst = ncbTypeConvertor::ToTarget<ANYT>::Get(&dst);
	}
private:
	T dst;
};

NCB_SET_CONVERTOR_DST(Vector2, Vector2Convertor);
NCB_REGISTER_SUBCLASS_DELAY(Vector2) {
	NCB_CONSTRUCTOR((float, float));
	NCB_MEMBER_PROPERTY(x, float, x);
	NCB_MEMBER_PROPERTY(y, float, y);
	NCB_METHOD_VOID2(set, float, float);
	NCB_METHOD_VOID1(setScalar, float);
	NCB_METHOD_VOID1(copy, const Vector2&);
	NCB_METHOD_VOID1(add, const Vector2&);
	NCB_METHOD_VOID1(addScalar, float);
	NCB_METHOD_VOID1(sub, const Vector2&);
	NCB_METHOD_VOID1(subScalar, float);
	NCB_METHOD_VOID1(multiplyScalar, float);
	NCB_METHOD_VOID1(divideScalar, float);
	NCB_METHOD_VOID0(negate);
	NCB_METHOD(dot);
	NCB_METHOD(cross);
	NCB_METHOD(lengthSq);
	NCB_METHOD(length);
	NCB_METHOD_VOID0(normalize);
	NCB_METHOD(distanceTo);
	NCB_METHOD(distanceToSquared);
	NCB_METHOD_VOID2(lerp, const Vector2&, float);
	NCB_METHOD(equals);
	NCB_METHOD_VOID0(floor);
	NCB_METHOD_VOID0(ceil);
	NCB_METHOD_VOID0(round);
};

// ------------------------------------------------------- Vector3
template <class T>
struct Vector3Convertor {
	typedef ncbInstanceAdaptor<T> AdaptorT;
	template <typename ANYT>
	void operator ()(ANYT &adst, const tTJSVariant &src) {
		if (src.Type() == tvtObject) {
			T *obj = AdaptorT::GetNativeInstance(src.AsObjectNoAddRef());
			if (obj) {
				dst = *obj;
			} else {
				ncbPropAccessor info(src);
				if (IsArray(src)) {
					dst = Vector3((float)info.getRealValue(0),
								  (float)info.getRealValue(1),
								  (float)info.getRealValue(2));
				} else {
					dst = Vector3((float)info.getRealValue(TJS_W("x")),
								  (float)info.getRealValue(TJS_W("y")),
								  (float)info.getRealValue(TJS_W("z")));
				}
			}
		} else {
			dst = T();
		}
		adst = ncbTypeConvertor::ToTarget<ANYT>::Get(&dst);
	}
private:
	T dst;
};

NCB_SET_CONVERTOR_DST(Vector3, Vector3Convertor);
NCB_REGISTER_SUBCLASS_DELAY(Vector3) {
	NCB_CONSTRUCTOR((float, float, float));
	NCB_MEMBER_PROPERTY(x, float, x);
	NCB_MEMBER_PROPERTY(y, float, y);
	NCB_MEMBER_PROPERTY(z, float, z);
	NCB_METHOD_VOID3(set, float, float, float);
	NCB_METHOD_VOID1(setScalar, float);
	NCB_METHOD_VOID1(setX, float);
	NCB_METHOD_VOID1(setY, float);
	NCB_METHOD_VOID1(setZ, float);
	NCB_METHOD_VOID1(copy, const Vector3&);
	NCB_METHOD_VOID1(add, const Vector3&);
	NCB_METHOD_VOID1(addScalar, float);
	NCB_METHOD_VOID2(addVectors, const Vector3&, const Vector3&);
	NCB_METHOD_VOID2(addScaledVector, const Vector3&, float);
	NCB_METHOD_VOID1(sub, const Vector3&);
	NCB_METHOD_VOID1(subScalar, float);
	NCB_METHOD_VOID2(subVectors, const Vector3&, const Vector3&);
	NCB_METHOD_VOID1(multiply, const Vector3&);
	NCB_METHOD_VOID1(multiplyScalar, float);
	NCB_METHOD_VOID2(multiplyVectors, const Vector3&, const Vector3&);
	NCB_METHOD_VOID2(applyAxisAngle, const Vector3&, float);
	NCB_METHOD_VOID1(applyEuler, const Euler&);
	NCB_METHOD_VOID1(applyMatrix3, const Matrix3&);
	NCB_METHOD_VOID1(applyMatrix4, const Matrix4&);
	NCB_METHOD_VOID1(applyQuaternion, const Quaternion&);
	NCB_METHOD_VOID1(project, const Camera&);
	NCB_METHOD_VOID1(unproject, const Camera&);
	NCB_METHOD_VOID1(transformDirection, const Matrix4&);
	NCB_METHOD_VOID1(divide, const Vector3&);
	NCB_METHOD_VOID1(divideScalar, float);
	NCB_METHOD_VOID1(min, const Vector3&);
	NCB_METHOD_VOID1(max, const Vector3&);
	NCB_METHOD_VOID2(clamp, const Vector3&, const Vector3&);
	NCB_METHOD_VOID2(clampScalar, float, float);
	NCB_METHOD_VOID2(clampLength, float, float);
	NCB_METHOD_VOID0(floor);
	NCB_METHOD_VOID0(ceil);
	NCB_METHOD_VOID0(round);
	NCB_METHOD_VOID0(negate);
	NCB_METHOD(dot);
	NCB_METHOD(lengthSq);
	NCB_METHOD(length);
	NCB_METHOD(manhattanLength);
	NCB_METHOD_VOID0(normalize);
	NCB_METHOD_VOID1(setLength, float);
	NCB_METHOD_VOID2(lerp, const Vector3&, float);
	NCB_METHOD_VOID3(lerpVectors, const Vector3&, const Vector3&, float);
	NCB_METHOD_VOID1(cross, const Vector3&);
	NCB_METHOD_VOID2(crossVectors, const Vector3&, const Vector3&);
	NCB_METHOD_VOID1(reflect, const Vector3&);
	NCB_METHOD(angleTo);
	NCB_METHOD(distanceTo);
	NCB_METHOD(distanceToSquared);
	NCB_METHOD(manhattanDistanceTo);
	NCB_METHOD_VOID1(setFromMatrixPosition, const Matrix4&);
	NCB_METHOD_VOID1(setFromMatrixScale, const Matrix4&);
	NCB_METHOD(equals);
};

// ------------------------------------------------------- Color
template <class T>
struct ColorConvertor {
	typedef ncbInstanceAdaptor<T> AdaptorT;
	template <typename ANYT>
	void operator ()(ANYT &adst, const tTJSVariant &src) {
		if (src.Type() == tvtObject) {
			T *obj = AdaptorT::GetNativeInstance(src.AsObjectNoAddRef());
			if (obj) {
				dst = *obj;
			} else {
				ncbPropAccessor info(src);
				if (IsArray(src)) {
					dst = Color((float)info.getRealValue(0),
								(float)info.getRealValue(1),
								(float)info.getRealValue(2));
				} else {
					dst = Color((float)info.getRealValue(TJS_W("r")),
								(float)info.getRealValue(TJS_W("g")),
								(float)info.getRealValue(TJS_W("b")));
				}
			}
		} else if (src.Type() == tvtInteger) {
			dst = Color((unsigned int)src.AsInteger());
		} else {
			dst = T();
		}
		adst = ncbTypeConvertor::ToTarget<ANYT>::Get(&dst);
	}
private:
	T dst;
};

NCB_SET_CONVERTOR_DST(Color, ColorConvertor);
NCB_REGISTER_SUBCLASS_DELAY(Color) {
	NCB_CONSTRUCTOR((float, float, float));
	NCB_MEMBER_PROPERTY(r, float, r);
	NCB_MEMBER_PROPERTY(g, float, g);
	NCB_MEMBER_PROPERTY(b, float, b);
	NCB_METHOD_VOID1(setScalar, float);
	NCB_METHOD_VOID3(setRGB, float, float, float);
	NCB_METHOD_VOID3(setHSL, float, float, float);
	NCB_METHOD_VOID1(setHex, unsigned int);
	NCB_METHOD_VOID1(copy, const Color&);
	NCB_METHOD_VOID1(add, const Color&);
	NCB_METHOD_VOID2(addColors, const Color&, const Color&);
	NCB_METHOD_VOID1(addScalar, float);
	NCB_METHOD_VOID1(sub, const Color&);
	NCB_METHOD_VOID1(multiply, const Color&);
	NCB_METHOD_VOID1(multiplyScalar, float);
	NCB_METHOD_VOID2(lerp, const Color&, float);
	NCB_METHOD_VOID3(lerpColors, const Color&, const Color&, float);
	NCB_METHOD_VOID2(lerpHSL, const Color&, float);
	NCB_METHOD_VOID0(randomize);
	NCB_METHOD(getHex);
	NCB_METHOD(getHexString);
	NCB_METHOD(getStyle);
	NCB_METHOD(equals);
};

// ------------------------------------------------------- Euler

// Euler proxy functions (avoid std::optional params and Euler& return)
static void Euler_set(Euler *self, float x, float y, float z) { self->set(x, y, z); }
static void Euler_setFromRotationMatrix(Euler *self, const Matrix4& m) { self->setFromRotationMatrix(m); }
static void Euler_setFromQuaternion(Euler *self, const Quaternion& q) { self->setFromQuaternion(q); }
static void Euler_setFromVector3(Euler *self, const Vector3& v) { self->setFromVector3(v); }

NCB_REGISTER_SUBCLASS(Euler) {
	NCB_CONSTRUCTOR((float, float, float));
	NCB_MEMBER_PROPERTY_RO(x, float, x);
	NCB_MEMBER_PROPERTY_RO(y, float, y);
	NCB_MEMBER_PROPERTY_RO(z, float, z);
	NCB_METHOD(getOrder);
	NCB_METHOD(setOrder);
	NCB_METHOD_PROXY(set, Euler_set);
	NCB_METHOD_VOID1(copy, const Euler&);
	NCB_METHOD_PROXY(setFromRotationMatrix, Euler_setFromRotationMatrix);
	NCB_METHOD_PROXY(setFromQuaternion, Euler_setFromQuaternion);
	NCB_METHOD_PROXY(setFromVector3, Euler_setFromVector3);
	NCB_METHOD(equals);
};

// ------------------------------------------------------- Quaternion
NCB_REGISTER_SUBCLASS(Quaternion) {
	NCB_CONSTRUCTOR((float, float, float, float));
	NCB_MEMBER_PROPERTY_RO(x, float, x);
	NCB_MEMBER_PROPERTY_RO(y, float, y);
	NCB_MEMBER_PROPERTY_RO(z, float, z);
	NCB_MEMBER_PROPERTY_RO(w, float, w);
	NCB_METHOD_VOID4(set, float, float, float, float);
	NCB_METHOD_VOID1(copy, const Quaternion&);
	NCB_METHOD_VOID2(setFromEuler, const Euler&, bool);
	NCB_METHOD_VOID2(setFromAxisAngle, const Vector3&, float);
	NCB_METHOD_VOID1(setFromRotationMatrix, const Matrix4&);
	NCB_METHOD_VOID2(setFromUnitVectors, const Vector3&, const Vector3&);
	NCB_METHOD(angleTo);
	NCB_METHOD_VOID2(rotateTowards, const Quaternion&, float);
	NCB_METHOD_VOID0(identity);
	NCB_METHOD_VOID0(invert);
	NCB_METHOD_VOID0(conjugate);
	NCB_METHOD(dot);
	NCB_METHOD(lengthSq);
	NCB_METHOD(length);
	NCB_METHOD_VOID0(normalize);
	NCB_METHOD_VOID1(multiply, const Quaternion&);
	NCB_METHOD_VOID1(premultiply, const Quaternion&);
	NCB_METHOD_VOID2(multiplyQuaternions, const Quaternion&, const Quaternion&);
	NCB_METHOD_VOID2(slerp, const Quaternion&, float);
	NCB_METHOD_VOID3(slerpQuaternions, const Quaternion&, const Quaternion&, float);
	NCB_METHOD(equals);
};

// ------------------------------------------------------- Matrix4
// Matrix4 proxy for setPosition(const Vector3&) overload
static void Matrix4_setPosition(Matrix4 *self, const Vector3& v) { self->setPosition(v); }
// Matrix4 proxy for makeTranslation(const Vector3&) overload
static void Matrix4_makeTranslation(Matrix4 *self, const Vector3& v) { self->makeTranslation(v); }

NCB_REGISTER_SUBCLASS(Matrix4) {
	NCB_CONSTRUCTOR(());
	NCB_METHOD_VOID16(set, float, float, float, float, float, float, float, float, float, float, float, float, float, float, float, float);
	NCB_METHOD_VOID0(identity);
	NCB_METHOD_VOID1(copy, const Matrix4&);
	NCB_METHOD_VOID1(copyPosition, const Matrix4&);
	NCB_METHOD_VOID3(extractBasis, Vector3&, Vector3&, Vector3&);
	NCB_METHOD_VOID3(makeBasis, const Vector3&, const Vector3&, const Vector3&);
	NCB_METHOD_VOID1(extractRotation, const Matrix4&);
	NCB_METHOD_VOID1(makeRotationFromEuler, const Euler&);
	NCB_METHOD_VOID1(makeRotationFromQuaternion, const Quaternion&);
	NCB_METHOD_VOID3(lookAt, const Vector3&, const Vector3&, const Vector3&);
	NCB_METHOD_VOID1(multiply, const Matrix4&);
	NCB_METHOD_VOID1(premultiply, const Matrix4&);
	NCB_METHOD_VOID2(multiplyMatrices, const Matrix4&, const Matrix4&);
	NCB_METHOD_VOID1(multiplyScalar, float);
	NCB_METHOD(determinant);
	NCB_METHOD_VOID0(transpose);
	NCB_METHOD_PROXY(setPosition, Matrix4_setPosition);
	NCB_METHOD_VOID0(invert);
	NCB_METHOD_VOID1(scale, const Vector3&);
	NCB_METHOD(getMaxScaleOnAxis);
	NCB_METHOD_PROXY(makeTranslation, Matrix4_makeTranslation);
	NCB_METHOD_VOID1(makeRotationX, float);
	NCB_METHOD_VOID1(makeRotationY, float);
	NCB_METHOD_VOID1(makeRotationZ, float);
	NCB_METHOD_VOID2(makeRotationAxis, const Vector3&, float);
	NCB_METHOD_VOID3(makeScale, float, float, float);
	NCB_METHOD_VOID6(makeShear, float, float, float, float, float, float);
	NCB_METHOD_VOID3(compose, const Vector3&, const Quaternion&, const Vector3&);
	NCB_METHOD(decompose);
	NCB_METHOD_VOID6(makePerspective, float, float, float, float, float, float);
	NCB_METHOD_VOID6(makeOrthographic, float, float, float, float, float, float);
	NCB_METHOD(equals);
};

// ------------------------------------------------------- Matrix3
NCB_REGISTER_SUBCLASS(Matrix3) {
	NCB_CONSTRUCTOR(());
	NCB_METHOD_VOID9(set, float, float, float, float, float, float, float, float, float);
	NCB_METHOD_VOID0(identity);
	NCB_METHOD_VOID1(copy, const Matrix3&);
	NCB_METHOD_VOID1(setFromMatrix4, const Matrix4&);
	NCB_METHOD_VOID1(multiply, const Matrix3&);
	NCB_METHOD_VOID1(premultiply, const Matrix3&);
	NCB_METHOD_VOID2(multiplyMatrices, const Matrix3&, const Matrix3&);
	NCB_METHOD_VOID1(multiplyScalar, float);
	NCB_METHOD(determinant);
	NCB_METHOD_VOID0(invert);
	NCB_METHOD_VOID0(transpose);
	NCB_METHOD_VOID1(getNormalMatrix, const Matrix4&);
	NCB_METHOD_VOID2(scale, float, float);
	NCB_METHOD_VOID1(rotate, float);
	NCB_METHOD_VOID2(translate, float, float);
	NCB_METHOD(equals);
};

// ------------------------------------------------------- Box3

static Vector3 Box3_getCenter(Box3 *self) { return self->getCenter(); }
static Vector3 Box3_getSize(Box3 *self) { return self->getSize(); }

static Vector3 Box3_getMin(Box3 *self) { return self->min(); }
static Vector3 Box3_getMax(Box3 *self) { return self->max(); }

NCB_REGISTER_SUBCLASS(Box3) {
	NCB_CONSTRUCTOR((const Vector3&, const Vector3&));
	Property(TJS_W("min"), &Box3_getMin, (int)0, Proxy);
	Property(TJS_W("max"), &Box3_getMax, (int)0, Proxy);
	NCB_METHOD_VOID2(set, const Vector3&, const Vector3&);
	NCB_METHOD(clone);
	NCB_METHOD_VOID1(copy, const Box3&);
	NCB_METHOD_VOID0(makeEmpty);
	NCB_METHOD(isEmpty);
	NCB_METHOD_PROXY(getCenter, Box3_getCenter);
	NCB_METHOD_PROXY(getSize, Box3_getSize);
	NCB_METHOD_VOID1(expandByPoint, const Vector3&);
	NCB_METHOD_VOID1(expandByVector, const Vector3&);
	NCB_METHOD_VOID1(expandByScalar, float);
	NCB_METHOD(containsPoint);
	NCB_METHOD(containsBox);
	NCB_METHOD(intersectsBox);
	NCB_METHOD(intersectsSphere);
	NCB_METHOD(intersectsPlane);
	NCB_METHOD(distanceToPoint);
	NCB_METHOD_VOID1(intersect, const Box3&);
	NCB_METHOD_VOID1(union_, const Box3&);
	NCB_METHOD_VOID1(applyMatrix4, const Matrix4&);
	NCB_METHOD_VOID1(translate, const Vector3&);
	NCB_METHOD(equals);
};

// ------------------------------------------------------- Sphere
NCB_REGISTER_SUBCLASS(Sphere) {
	NCB_CONSTRUCTOR((const Vector3&, float));
	NCB_METHOD_VOID2(set, const Vector3&, float);
	NCB_METHOD(clone);
	NCB_METHOD_VOID1(copy, const Sphere&);
	NCB_METHOD(isEmpty);
	NCB_METHOD_VOID0(makeEmpty);
	NCB_METHOD(containsPoint);
	NCB_METHOD(distanceToPoint);
	NCB_METHOD(intersectsSphere);
	NCB_METHOD(intersectsBox);
	NCB_METHOD(intersectsPlane);
	NCB_METHOD_VOID1(expandByPoint, const Vector3&);
	NCB_METHOD_VOID1(translate, const Vector3&);
	NCB_METHOD(equals);
};

// ------------------------------------------------------- Ray

static Vector3 Ray_at(Ray *self, float t) { Vector3 target; self->at(t, target); return target; }
static Vector3 Ray_closestPointToPoint(Ray *self, const Vector3& point) { Vector3 target; self->closestPointToPoint(point, target); return target; }
static Vector3 Ray_intersectSphere(Ray *self, const Sphere& sphere) { Vector3 target; self->intersectSphere(sphere, target); return target; }
static Vector3 Ray_intersectBox(Ray *self, const Box3& box) { Vector3 target; self->intersectBox(box, target); return target; }
static Vector3 Ray_intersectPlane(Ray *self, const Plane& plane) { Vector3 target; self->intersectPlane(plane, target); return target; }

NCB_REGISTER_SUBCLASS(Ray) {
	NCB_CONSTRUCTOR((const Vector3&, const Vector3&));
	NCB_METHOD_VOID2(set, const Vector3&, const Vector3&);
	NCB_METHOD(clone);
	NCB_METHOD_VOID1(copy, const Ray&);
	NCB_METHOD_PROXY(at, Ray_at);
	NCB_METHOD_VOID1(lookAt, const Vector3&);
	NCB_METHOD_VOID1(recast, float);
	NCB_METHOD_PROXY(closestPointToPoint, Ray_closestPointToPoint);
	NCB_METHOD(distanceToPoint);
	NCB_METHOD(distanceSqToPoint);
	NCB_METHOD_PROXY(intersectSphere, Ray_intersectSphere);
	NCB_METHOD(intersectsSphere);
	NCB_METHOD_PROXY(intersectBox, Ray_intersectBox);
	NCB_METHOD(intersectsBox);
	NCB_METHOD_PROXY(intersectPlane, Ray_intersectPlane);
	NCB_METHOD(intersectsPlane);
	NCB_METHOD_VOID1(applyMatrix4, const Matrix4&);
	NCB_METHOD(equals);
};

// ------------------------------------------------------- Plane

static Vector3 Plane_coplanarPoint(Plane *self) { Vector3 t; self->coplanarPoint(t); return t; }

// Plane proxy for applyMatrix4 overload
static void Plane_applyMatrix4(Plane *self, const Matrix4& m) { self->applyMatrix4(m); }

NCB_REGISTER_SUBCLASS(Plane) {
	NCB_CONSTRUCTOR((const Vector3&, float));
	NCB_METHOD_VOID2(set, const Vector3&, float);
	NCB_METHOD_VOID4(setComponents, float, float, float, float);
	NCB_METHOD_VOID2(setFromNormalAndCoplanarPoint, const Vector3&, const Vector3&);
	NCB_METHOD_VOID1(copy, const Plane&);
	NCB_METHOD_VOID0(normalize);
	NCB_METHOD_VOID0(negate);
	NCB_METHOD(distanceToPoint);
	NCB_METHOD(distanceToSphere);
	NCB_METHOD_PROXY(coplanarPoint, Plane_coplanarPoint);
	NCB_METHOD_PROXY(applyMatrix4, Plane_applyMatrix4);
	NCB_METHOD_VOID1(translate, const Vector3&);
	NCB_METHOD(equals);
};

// ------------------------------------------------------- Box2

static Vector2 Box2_getCenter(Box2 *self) { return self->getCenter(); }
static Vector2 Box2_getSize(Box2 *self) { return self->getSize(); }
static void Box2_union(Box2 *self, const Box2& box) { self->_union(box); }

static Vector2 Box2_getMin(Box2 *self) { return self->getMin(); }
static Vector2 Box2_getMax(Box2 *self) { return self->getMax(); }

NCB_REGISTER_SUBCLASS(Box2) {
	NCB_CONSTRUCTOR((const Vector2&, const Vector2&));
	Property(TJS_W("min"), &Box2_getMin, (int)0, Proxy);
	Property(TJS_W("max"), &Box2_getMax, (int)0, Proxy);
	NCB_METHOD_VOID2(set, const Vector2&, const Vector2&);
	NCB_METHOD(clone);
	NCB_METHOD_VOID1(copy, const Box2&);
	NCB_METHOD_VOID0(makeEmpty);
	NCB_METHOD(isEmpty);
	NCB_METHOD_PROXY(getCenter, Box2_getCenter);
	NCB_METHOD_PROXY(getSize, Box2_getSize);
	NCB_METHOD_VOID1(expandByPoint, const Vector2&);
	NCB_METHOD_VOID1(expandByVector, const Vector2&);
	NCB_METHOD_VOID1(expandByScalar, float);
	NCB_METHOD(containsPoint);
	NCB_METHOD(containsBox);
	NCB_METHOD(intersectsBox);
	NCB_METHOD(distanceToPoint);
	NCB_METHOD_VOID1(intersect, const Box2&);
	NCB_METHOD_PROXY(union_, Box2_union);
	NCB_METHOD_VOID1(translate, const Vector2&);
	NCB_METHOD(equals);
};

// ------------------------------------------------------- Line3

static Vector3 Line3_getCenter(Line3 *self) { return self->getCenter(); }
static Vector3 Line3_delta(Line3 *self) { return self->delta(); }
static Vector3 Line3_at(Line3 *self, float t) { Vector3 target; self->at(t, target); return target; }
static Vector3 Line3_closestPointToPoint(Line3 *self, const Vector3& point, bool clampToLine) {
	Vector3 target;
	self->closestPointToPoint(point, clampToLine, target);
	return target;
}

static Vector3 Line3_getStart(Line3 *self) { return self->start(); }
static Vector3 Line3_getEnd(Line3 *self) { return self->end(); }

NCB_REGISTER_SUBCLASS(Line3) {
	NCB_CONSTRUCTOR((const Vector3&, const Vector3&));
	Property(TJS_W("start"), &Line3_getStart, (int)0, Proxy);
	Property(TJS_W("end"), &Line3_getEnd, (int)0, Proxy);
	NCB_METHOD_VOID2(set, const Vector3&, const Vector3&);
	NCB_METHOD_VOID1(copy, const Line3&);
	NCB_METHOD_PROXY(getCenter, Line3_getCenter);
	NCB_METHOD_PROXY(delta, Line3_delta);
	NCB_METHOD(distanceSq);
	NCB_METHOD(distance);
	NCB_METHOD_PROXY(at, Line3_at);
	NCB_METHOD_PROXY(closestPointToPoint, Line3_closestPointToPoint);
	NCB_METHOD(closestPointToPointParameter);
	NCB_METHOD_VOID1(applyMatrix4, const Matrix4&);
};

// ------------------------------------------------------- Triangle

static Vector3 Triangle_getMidpoint(Triangle *self) { Vector3 t; self->getMidpoint(t); return t; }
static Vector3 Triangle_getNormal(Triangle *self) { Vector3 t; self->getNormal(t); return t; }
static Vector3 Triangle_getBarycoord(Triangle *self, Vector3 point) { Vector3 t; self->getBarycoord(point, t); return t; }
static Vector3 Triangle_closestPointToPoint(Triangle *self, const Vector3& p) { Vector3 t; self->closestPointToPoint(p, t); return t; }
static bool Triangle_containsPoint(Triangle *self, const Vector3& point) { return self->containsPoint(point); }
static bool Triangle_isFrontFacing(Triangle *self, const Vector3& dir) { return self->isFrontFacing(dir); }

static Vector3 Triangle_getA(Triangle *self) { return self->a(); }
static Vector3 Triangle_getB(Triangle *self) { return self->b(); }
static Vector3 Triangle_getC(Triangle *self) { return self->c(); }

NCB_REGISTER_SUBCLASS(Triangle) {
	NCB_CONSTRUCTOR((const Vector3&, const Vector3&, const Vector3&));
	Property(TJS_W("a"), &Triangle_getA, (int)0, Proxy);
	Property(TJS_W("b"), &Triangle_getB, (int)0, Proxy);
	Property(TJS_W("c"), &Triangle_getC, (int)0, Proxy);
	NCB_METHOD_VOID3(set, const Vector3&, const Vector3&, const Vector3&);
	NCB_METHOD(getArea);
	NCB_METHOD_PROXY(getMidpoint, Triangle_getMidpoint);
	NCB_METHOD_PROXY(getNormal, Triangle_getNormal);
	NCB_METHOD_PROXY(getBarycoord, Triangle_getBarycoord);
	NCB_METHOD_PROXY(containsPoint, Triangle_containsPoint);
	NCB_METHOD_PROXY(isFrontFacing, Triangle_isFrontFacing);
	NCB_METHOD_PROXY(closestPointToPoint, Triangle_closestPointToPoint);
};

// ------------------------------------------------------- Spherical
NCB_REGISTER_SUBCLASS(Spherical) {
	NCB_CONSTRUCTOR((float, float, float));
	NCB_MEMBER_PROPERTY(radius, float, radius);
	NCB_MEMBER_PROPERTY(phi, float, phi);
	NCB_MEMBER_PROPERTY(theta, float, theta);
	NCB_METHOD_VOID3(set, float, float, float);
	NCB_METHOD_VOID1(copy, const Spherical&);
	NCB_METHOD_VOID0(makeSafe);
	NCB_METHOD_VOID1(setFromVector3, const Vector3&);
	NCB_METHOD_VOID3(setFromCartesianCoords, float, float, float);
};

// ------------------------------------------------------- Cylindrical
NCB_REGISTER_SUBCLASS(Cylindrical) {
	NCB_CONSTRUCTOR((float, float, float));
	NCB_PROPERTY_RO(radius, radius);
	NCB_PROPERTY_RO(theta, theta);
	NCB_PROPERTY_RO(y, y);
	NCB_METHOD_VOID3(set, float, float, float);
	NCB_METHOD_VOID1(copy, const Cylindrical&);
	NCB_METHOD_VOID1(setFromVector3, const Vector3&);
	NCB_METHOD_VOID3(setFromCartesianCoords, float, float, float);
};

// ======================================================================
// threepp shared_ptr ラッピング用テンプレートクラス
// ======================================================================

// ----------------------------------------------------------------------
// is-a 単一継承マップ (threepp のクラス階層を ThreeWrapper へ写す)
//
// ncbind の NCB_REGISTER_SUBCLASS_OF (単一継承) は C++ の実継承
// static_cast<Base*>(derived) に立脚するため、ThreeWrapper<Derived> を
// ThreeWrapper<Base> の C++ 派生にする必要がある。ThreeBaseOf<T> で threepp の
// 直近基底を宣言し、ThreeWrapper<T> はその基底ラッパを継承する。
// 各レベルは自分の型の shared_ptr を持ち、setObject で基底へ upcast 伝播する
// ので、基底 classid の BridgeFunctor が正しい raw ポインタを返す。
// ----------------------------------------------------------------------

// T の直近基底 threepp クラス。ルート (Object3D/Material/BufferGeometry/Texture 等)
// は void。ここに追記したチェーンだけが is-a 化される (未記載はフラット据え置き)。
template <class T> struct ThreeBaseOf { typedef void Type; };

template <class T> class ThreeWrapper;          //< 前方宣言
class ThreeWrapperRoot;                         //< ルートラッパの空基底

// C++ 継承用の基底ラッパ型 (基底が void ならルート空基底)
template <class B> struct ThreeCppBaseWrapper       { typedef ThreeWrapper<B> Type; };
template <>        struct ThreeCppBaseWrapper<void>  { typedef ThreeWrapperRoot Type; };
template <class T> struct ThreeWrapperBase { typedef typename ThreeCppBaseWrapper<typename ThreeBaseOf<T>::Type>::Type Type; };

// ncbind へ渡す is-a 基底型 (基底が void なら「継承なし」を表す void のまま)
template <class B> struct ThreeSubClassBase       { typedef ThreeWrapper<B> Type; };
template <>        struct ThreeSubClassBase<void>  { typedef void Type; };
template <class X> struct ThreeIsVoid       { enum { value = 0 }; };
template <>        struct ThreeIsVoid<void> { enum { value = 1 }; };

// --- is-a 継承マップ本体 ---
// 各ルート (Object3D / Material / BufferGeometry) は void。ここを増やすとチェーンが伸びる。
// Object3D 系
template <> struct ThreeBaseOf<Scene>              { typedef Object3D Type; };
template <> struct ThreeBaseOf<Group>              { typedef Object3D Type; };
template <> struct ThreeBaseOf<Camera>             { typedef Object3D Type; };
template <> struct ThreeBaseOf<PerspectiveCamera>  { typedef Camera   Type; };
template <> struct ThreeBaseOf<OrthographicCamera> { typedef Camera   Type; };
template <> struct ThreeBaseOf<Mesh>               { typedef Object3D Type; };
// Material 系 (threepp の Material は仮想基底。伝播は暗黙アップキャストで対応)
template <> struct ThreeBaseOf<MeshBasicMaterial>    { typedef Material Type; };
template <> struct ThreeBaseOf<MeshStandardMaterial> { typedef Material Type; };
// BufferGeometry 系 (全ジオメトリを is-a 化しないと Mesh() の geometry 抽出で
// GetNativeInstance<ThreeWrapper<BufferGeometry>> が null になり空メッシュになる)
template <> struct ThreeBaseOf<BoxGeometry>         { typedef BufferGeometry Type; };
template <> struct ThreeBaseOf<SphereGeometry>      { typedef BufferGeometry Type; };
template <> struct ThreeBaseOf<PlaneGeometry>       { typedef BufferGeometry Type; };
template <> struct ThreeBaseOf<CylinderGeometry>    { typedef BufferGeometry Type; };
template <> struct ThreeBaseOf<ConeGeometry>        { typedef BufferGeometry Type; };
template <> struct ThreeBaseOf<TorusGeometry>       { typedef BufferGeometry Type; };
template <> struct ThreeBaseOf<TorusKnotGeometry>   { typedef BufferGeometry Type; };
template <> struct ThreeBaseOf<CircleGeometry>      { typedef BufferGeometry Type; };
template <> struct ThreeBaseOf<RingGeometry>        { typedef BufferGeometry Type; };
template <> struct ThreeBaseOf<IcosahedronGeometry> { typedef BufferGeometry Type; };
template <> struct ThreeBaseOf<OctahedronGeometry>  { typedef BufferGeometry Type; };
template <> struct ThreeBaseOf<CapsuleGeometry>     { typedef BufferGeometry Type; };
template <> struct ThreeBaseOf<LatheGeometry>       { typedef BufferGeometry Type; };
template <> struct ThreeBaseOf<EdgesGeometry>       { typedef BufferGeometry Type; };
template <> struct ThreeBaseOf<WireframeGeometry>   { typedef BufferGeometry Type; };
template <> struct ThreeBaseOf<ShapeGeometry>       { typedef BufferGeometry Type; };
template <> struct ThreeBaseOf<ExtrudeGeometry>     { typedef BufferGeometry Type; };
// Texture 系 (Texture がルート)
template <> struct ThreeBaseOf<DataTexture>        { typedef Texture Type; };

// ルートラッパの空基底 (実体の所有/非所有は adaptor 側が管理する)
class ThreeWrapperRoot {
public:
	ThreeWrapperRoot() {}
	virtual ~ThreeWrapperRoot() {}
};

// setObject 時に、基底ラッパ部分へ同一実体を upcast 伝播する (基底へ再帰)
template <class T, class B = typename ThreeBaseOf<T>::Type>
struct ThreeWrapperAssignBase {
	static void assign(ThreeWrapper<T> *self, const std::shared_ptr<T> &src);
};
template <class T>
struct ThreeWrapperAssignBase<T, void> {
	static void assign(ThreeWrapper<T> *, const std::shared_ptr<T> &) {}
};

template <class T>
class ThreeWrapper : public ThreeWrapperBase<T>::Type {
	typedef T ThreeClassT;
	typedef ThreeWrapper<ThreeClassT> WrapperT;
	typedef typename ThreeWrapperBase<T>::Type BaseWrapperT;
protected:
	std::shared_ptr<ThreeClassT> obj;
public:
	ThreeWrapper() : obj(nullptr) {}
	ThreeWrapper(std::shared_ptr<ThreeClassT> src) : obj(nullptr) { setObject(src); }
	ThreeWrapper(const WrapperT &orig) : obj(nullptr) { setObject(orig.obj); }
	~ThreeWrapper() {}

	std::shared_ptr<ThreeClassT> getObject() { return obj; }
	ThreeClassT* getRawPtr() { return obj.get(); }

	void setObject(std::shared_ptr<ThreeClassT> src) {
		obj = src;
		// 基底ラッパ部分にも upcast した同一実体を持たせる (基底 classid 用)
		ThreeWrapperAssignBase<ThreeClassT>::assign(this, src);
	}

	struct BridgeFunctor {
		ThreeClassT* operator()(WrapperT *p) const {
			return p->getRawPtr();
		}
	};
};

// 非ルート: 基底ラッパへ upcast した実体を setObject (さらに上へ再帰)。
// 暗黙アップキャスト (shared_ptr<B> の変換コンストラクタ) を使うので、
// threepp の仮想継承 (例: MeshBasicMaterial : public virtual Material) でも成立する
// (static_pointer_cast は仮想基底へは不可なため使わない)。
template <class T, class B>
void ThreeWrapperAssignBase<T, B>::assign(ThreeWrapper<T> *self, const std::shared_ptr<T> &src) {
	static_cast<ThreeWrapper<B>*>(self)->setObject(std::shared_ptr<B>(src));
}

// ThreeWrapper<T> の is-a 基底を ncbind に伝える (ThreeBaseOf に基づき自動導出)。
// フラットなクラス (ThreeBaseOf=void) は BaseT=void で従来どおり非継承扱い。
template <class T>
struct ncbSubClassOf<ThreeWrapper<T> > {
	typedef typename ThreeSubClassBase<typename ThreeBaseOf<T>::Type>::Type BaseT;
	enum { HasBase = !ThreeIsVoid<BaseT>::value };
};

// コンバータ登録用マクロ
#define NCB_THREE_CONVERTOR(type) \
NCB_SET_CONVERTOR(type*, ThreeTypeConvertor<type>);\
NCB_SET_CONVERTOR(const type*, ThreeTypeConvertor<const type>);\
NCB_SET_CONVERTOR(std::shared_ptr<type>, ThreeSharedPtrConvertor<type>)

// shared_ptr のコンバータ
template <class T>
struct ThreeSharedPtrConvertor {
	typedef typename ncbTypeConvertor::Stripper<T>::Type ThreeClassT;
	typedef std::shared_ptr<ThreeClassT> SharedPtrT;
	typedef ThreeWrapper<ThreeClassT> WrapperT;
	typedef ncbInstanceAdaptor<WrapperT> AdaptorT;
protected:
	SharedPtrT result;
public:
	ThreeSharedPtrConvertor() : result(nullptr) {}
	
	void operator ()(SharedPtrT &dst, const tTJSVariant &src) {
		WrapperT *obj;
		if (src.Type() == tvtObject && (obj = AdaptorT::GetNativeInstance(src.AsObjectNoAddRef()))) {
			dst = obj->getObject();
		} else {
			dst = nullptr;
		}
	}

	void operator ()(tTJSVariant &dst, const SharedPtrT &src) {
		if (src != nullptr) {
			iTJSDispatch2 *adpobj = AdaptorT::CreateAdaptor(new WrapperT(src));
			if (adpobj) {
				dst = tTJSVariant(adpobj, adpobj);
				adpobj->Release();
			} else {
				dst.Clear();
			}
		} else {
			dst.Clear();
		}
	}
};

// rawptr のコンバータ
template <class T>
struct ThreeTypeConvertor {
	typedef typename ncbTypeConvertor::Stripper<T>::Type ThreeClassT;
	typedef T *ThreeClassP;
	typedef ThreeWrapper<ThreeClassT> WrapperT;
	typedef ncbInstanceAdaptor<WrapperT> AdaptorT;
protected:
	ThreeClassT *result;
public:
	ThreeTypeConvertor() : result(nullptr) {}
	
	void operator ()(ThreeClassP &dst, const tTJSVariant &src) {
		WrapperT *obj;
		if (src.Type() == tvtObject && (obj = AdaptorT::GetNativeInstance(src.AsObjectNoAddRef()))) {
			dst = obj->getRawPtr();
		} else {
			dst = nullptr;
		}
	}

	void operator ()(tTJSVariant &dst, const ThreeClassP &src) {
		// rawptr から variant への変換は基本的に使わない(shared_ptrを使う)
		dst.Clear();
	}
};

// ラッピング処理用マクロ
// is-a 継承時 (ThreeBaseOf<Class> が非 void) は、全クラス登録後 (PostRegist) に
// 基底クラスオブジェクトのメンバを派生へコピーする。フラットなクラスでは
// ncbSubClassMemberCopy<...,void>::Copy() が no-op なので無条件に emit してよい。
#define NCB_REGISTER_THREE_SUBCLASS(Class) \
	NCB_THREE_CONVERTOR(Class); \
	static void ncbThreeMemberCopy_ ## Class () { ncbSubClassMemberCopy<ThreeWrapper<Class> >::Copy(); } \
	NCB_POST_REGIST_CALLBACK(ncbThreeMemberCopy_ ## Class); \
	NCB_REGISTER_SUBCLASS(ThreeWrapper<Class>) { typedef Class ThreeClass;
#define NCB_THREE_METHOD(name)  Method(TJS_W(# name), &ThreeClass::name, Bridge<ThreeWrapper<ThreeClass>::BridgeFunctor>())
#define NCB_THREE_MCAST(ret, method, args) static_cast<ret (ThreeClass::*) args>(&ThreeClass::method)
#define NCB_THREE_METHOD2(name, ret, method, args) Method(TJS_W(# name), NCB_THREE_MCAST(ret, method, args), Bridge<ThreeWrapper<ThreeClass>::BridgeFunctor>())
#define NCB_THREE_PROPERTY(name, get, set)  Property(TJS_W(# name), &ThreeClass::get, &ThreeClass::set, Bridge<ThreeWrapper<ThreeClass>::BridgeFunctor>())
#define NCB_THREE_PROPERTY_RO(name, get)  Property(TJS_W(# name), &ThreeClass::get, (int)0, getBridgeType(&ThreeWrapper<ThreeClass>::BridgeFunctor::operator()))
// free 関数 ProxyGet/ProxySet(ThreeClass* inst, ...) は第1引数にインスタンスを取る
// proxy 形式。Bridge(=ivtBridge)は inst を TJS 引数として数えてしまい「引数の数が不正」
// になるため、ArgsCount を -1 する ProxyBridge(=ivtProxy<ivtBridge>)を使う。
#define NCB_THREE_MEMBER_PROPERTY(name, type, membername) \
	struct AutoProp_ ## name { \
		static void ProxySet(ThreeClass *inst, type value) { inst->membername = value; } \
		static type ProxyGet(ThreeClass *inst) {      return inst->membername; } }; \
	Property(TJS_W(#name), AutoProp_ ## name::ProxyGet, AutoProp_ ## name::ProxySet, ProxyBridge<ThreeWrapper<ThreeClass>::BridgeFunctor>())
#define NCB_THREE_MEMBER_PROPERTY_RO(name, type, membername) \
	struct AutoProp_ ## name { \
		static type ProxyGet(ThreeClass *inst) {      return inst->membername; } }; \
	Property(TJS_W(#name), &AutoProp_ ## name::ProxyGet, (int)0, getProxyBridgeType(&ThreeWrapper<ThreeClass>::BridgeFunctor::operator()))

// ======================================================================
// Object3D (基底クラス)
// ======================================================================

static tjs_error
Object3DFactory(ThreeWrapper<Object3D> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	// Object3D自体はあまり直接生成しない(Groupを使う)が、念のため
	auto obj = std::make_shared<Object3D>();
	*result = new ThreeWrapper<Object3D>(obj);
	return TJS_S_OK;
}

static void Object3DAdd(ThreeWrapper<Object3D> *self, ThreeWrapper<Object3D> *child)
{
	if (self && child && self->getObject() && child->getObject()) {
		self->getObject()->add(child->getObject());
	}
}

static void Object3DRemove(ThreeWrapper<Object3D> *self, ThreeWrapper<Object3D> *child)
{
	if (self && child && self->getObject() && child->getRawPtr()) {
		self->getObject()->remove(*child->getRawPtr());
	}
}

static void Object3DLookAt(ThreeWrapper<Object3D> *self, float x, float y, float z)
{
	if (self && self->getObject()) {
		self->getObject()->lookAt(x, y, z);
	}
}

// Object3D proxy functions for methods returning Object3D& (chaining)
static void Object3D_rotateX(ThreeWrapper<Object3D> *self, float angle) { if (self && self->getObject()) self->getObject()->rotateX(angle); }
static void Object3D_rotateY(ThreeWrapper<Object3D> *self, float angle) { if (self && self->getObject()) self->getObject()->rotateY(angle); }
static void Object3D_rotateZ(ThreeWrapper<Object3D> *self, float angle) { if (self && self->getObject()) self->getObject()->rotateZ(angle); }
static void Object3D_rotateOnAxis(ThreeWrapper<Object3D> *self, const Vector3& axis, float angle) { if (self && self->getObject()) self->getObject()->rotateOnAxis(axis, angle); }
static void Object3D_rotateOnWorldAxis(ThreeWrapper<Object3D> *self, const Vector3& axis, float angle) { if (self && self->getObject()) self->getObject()->rotateOnWorldAxis(axis, angle); }
static void Object3D_translateX(ThreeWrapper<Object3D> *self, float distance) { if (self && self->getObject()) self->getObject()->translateX(distance); }
static void Object3D_translateY(ThreeWrapper<Object3D> *self, float distance) { if (self && self->getObject()) self->getObject()->translateY(distance); }
static void Object3D_translateZ(ThreeWrapper<Object3D> *self, float distance) { if (self && self->getObject()) self->getObject()->translateZ(distance); }
static void Object3D_translateOnAxis(ThreeWrapper<Object3D> *self, const Vector3& axis, float distance) { if (self && self->getObject()) self->getObject()->translateOnAxis(axis, distance); }

// Object3D のプロキシ取得関数
static ttstr Object3DGetType(ThreeWrapper<Object3D> *self) {
	if (self && self->getObject()) {
		return ttstr(self->getObject()->type().c_str());
	}
	return TJS_W("");
}

static unsigned int Object3DGetId(ThreeWrapper<Object3D> *self) {
	if (self && self->getObject()) {
		return self->getObject()->id;
	}
	return 0;
}

static ttstr Object3DGetName(ThreeWrapper<Object3D> *self) {
	if (self && self->getObject()) {
		// std::string -> ttstr
		const auto& s = self->getObject()->name;
		std::wstring ws(s.begin(), s.end());
		return ttstr(ws.c_str());
	}
	return TJS_W("");
}

static void Object3DSetName(ThreeWrapper<Object3D> *self, ttstr name) {
	if (self && self->getObject()) {
		const tjs_char* ws = name.c_str();
		std::string s;
		while (*ws) {
			if (*ws < 128) {
				s += (char)*ws;
			} else {
				s += '?';
			}
			ws++;
		}
		self->getObject()->name = s;
	}
}

// position, rotation, quaternion, scale, up は参照型の戻り値で ncbind に対応できないため省略

// ======================================================================
// Object3D 共通メンバの継承代替 (ncbind は flat クラス登録で親子継承を張らない)
//
// Camera/Light/Scene/Group 等の Object3D 派生クラスは、自分の登録ブロックに
// THREE_OBJECT3D_MEMBERS() を書くことで position/add/lookAt 等の Object3D 共通
// メンバを得る。プロパティ getter とメソッド proxy は ThreeClass でテンプレート化し、
// 各クラスが正しい型の自前アクセサを持つ (ThreeWrapper<T> は T 毎に別型のため)。
// ======================================================================

// 値プロパティ getter (bridge 経由。inst は raw な T*)
template <class T> struct ThreeObj3DGet {
	static Vector3    position(T *i)   { return i->position; }
	static Euler      rotation(T *i)   { return i->rotation; }
	static Quaternion quaternion(T *i) { return i->quaternion; }
	static Vector3    scale(T *i)      { return i->scale; }
	static Vector3    up(T *i)         { return i->up; }
};

// 任意の登録済み ThreeWrapper<派生> を持つ TJS オブジェクトから、基底 Object3D の
// shared_ptr を取り出す (所有権を保つ)。add(child) の wrapper 跨ぎ受け渡し用。
static std::shared_ptr<Object3D> ThreeExtractObject3D(const tTJSVariant &v) {
	if (v.Type() != tvtObject) return nullptr;
	iTJSDispatch2 *obj = v.AsObjectNoAddRef();
	if (!obj) return nullptr;
#define THREE_TRY_OBJ3D(TYPE) \
	if (auto *w = ncbInstanceAdaptor<ThreeWrapper<TYPE>>::GetNativeInstance(obj)) \
		if (auto sp = w->getObject()) return std::static_pointer_cast<Object3D>(sp);
	// 具体型を先に、基底を後に試す
	THREE_TRY_OBJ3D(Scene)
	THREE_TRY_OBJ3D(Group)
	THREE_TRY_OBJ3D(SkinnedMesh)
	THREE_TRY_OBJ3D(InstancedMesh)
	THREE_TRY_OBJ3D(Mesh)
	THREE_TRY_OBJ3D(Bone)
	THREE_TRY_OBJ3D(LOD)
	THREE_TRY_OBJ3D(PerspectiveCamera)
	THREE_TRY_OBJ3D(OrthographicCamera)
	THREE_TRY_OBJ3D(Camera)
	THREE_TRY_OBJ3D(AmbientLight)
	THREE_TRY_OBJ3D(DirectionalLight)
	THREE_TRY_OBJ3D(PointLight)
	THREE_TRY_OBJ3D(SpotLight)
	THREE_TRY_OBJ3D(HemisphereLight)
	THREE_TRY_OBJ3D(Light)
	THREE_TRY_OBJ3D(Object3D)
#undef THREE_TRY_OBJ3D
	return nullptr;
}

// 任意の登録済み Camera 派生 wrapper から shared_ptr<Camera> を取り出す。
// GLRenderer.render(scene, camera) の camera 引数の wrapper 跨ぎ用。
static std::shared_ptr<Camera> ThreeExtractCamera(const tTJSVariant &v) {
	if (v.Type() != tvtObject) return nullptr;
	iTJSDispatch2 *obj = v.AsObjectNoAddRef();
	if (!obj) return nullptr;
#define THREE_TRY_CAM(TYPE) \
	if (auto *w = ncbInstanceAdaptor<ThreeWrapper<TYPE>>::GetNativeInstance(obj)) \
		if (auto sp = w->getObject()) return std::static_pointer_cast<Camera>(sp);
	THREE_TRY_CAM(PerspectiveCamera)
	THREE_TRY_CAM(OrthographicCamera)
	THREE_TRY_CAM(Camera)
#undef THREE_TRY_CAM
	return nullptr;
}

// メソッド proxy (self をテンプレート化。child は wrapper 跨ぎのため tTJSVariant 受け)
template <class T> static void ThreeObjAdd(ThreeWrapper<T> *self, tTJSVariant child) {
	if (!self || !self->getObject()) return;
	if (auto c = ThreeExtractObject3D(child)) self->getObject()->add(c);
}
template <class T> static void ThreeObjRemove(ThreeWrapper<T> *self, tTJSVariant child) {
	if (!self || !self->getObject()) return;
	if (auto c = ThreeExtractObject3D(child)) self->getObject()->remove(*c);
}
template <class T> static void ThreeObjLookAt(ThreeWrapper<T> *self, float x, float y, float z) {
	if (self && self->getObject()) self->getObject()->lookAt(x, y, z);
}
template <class T> static void ThreeObjRotateX(ThreeWrapper<T> *self, float a) { if (self && self->getObject()) self->getObject()->rotateX(a); }
template <class T> static void ThreeObjRotateY(ThreeWrapper<T> *self, float a) { if (self && self->getObject()) self->getObject()->rotateY(a); }
template <class T> static void ThreeObjRotateZ(ThreeWrapper<T> *self, float a) { if (self && self->getObject()) self->getObject()->rotateZ(a); }
template <class T> static void ThreeObjTranslateX(ThreeWrapper<T> *self, float d) { if (self && self->getObject()) self->getObject()->translateX(d); }
template <class T> static void ThreeObjTranslateY(ThreeWrapper<T> *self, float d) { if (self && self->getObject()) self->getObject()->translateY(d); }
template <class T> static void ThreeObjTranslateZ(ThreeWrapper<T> *self, float d) { if (self && self->getObject()) self->getObject()->translateZ(d); }
// position/rotation/scale の getter は値コピー(RO)なので .set() では実体に反映されない。
// 実体を変更するには下の setter メソッドを使う (setPosition/setScale/setRotation)。
template <class T> static void ThreeObjSetPosition(ThreeWrapper<T> *self, float x, float y, float z) { if (self && self->getObject()) self->getObject()->position.set(x, y, z); }
template <class T> static void ThreeObjSetScale(ThreeWrapper<T> *self, float x, float y, float z) { if (self && self->getObject()) self->getObject()->scale.set(x, y, z); }
template <class T> static void ThreeObjSetRotation(ThreeWrapper<T> *self, float x, float y, float z) { if (self && self->getObject()) self->getObject()->rotation.set(x, y, z); }
// ワールド変換の取得 (matrixWorld から算出)。立ち絵フレーミングで head/hips の world 座標を
// 得るのに使う。matrixWorld が最新である必要があるため、呼ぶ前に scene を render するか
// updateMatrixWorld(true) を呼んでおくこと。
template <class T> static Vector3 ThreeObjGetWorldPosition(ThreeWrapper<T> *self) {
	Vector3 t;
	if (self && self->getObject()) self->getObject()->getWorldPosition(t);
	return t;
}
template <class T> static Quaternion ThreeObjGetWorldQuaternion(ThreeWrapper<T> *self) {
	Quaternion q;
	if (self && self->getObject()) self->getObject()->getWorldQuaternion(q);
	return q;
}
template <class T> static Vector3 ThreeObjGetWorldScale(ThreeWrapper<T> *self) {
	Vector3 t(1.f, 1.f, 1.f);
	if (self && self->getObject()) self->getObject()->getWorldScale(t);
	return t;
}

// --- ライトのシャドウ設定 (LightWithShadow を持つ Dir/Spot/Point 用) ---
// light.shadow (LightShadow) の主要パラメータと、影を落とす shadow camera の
// 範囲を proxy で公開する。ライト自身の castShadow は THREE_OBJECT3D_MEMBERS の
// castShadow プロパティで別途 true にする。
template <class T> static threepp::LightShadow *ThreeLightShadow(ThreeWrapper<T> *self)
{
	if (!self || !self->getObject()) return nullptr;
	auto *lws = static_cast<threepp::LightWithShadow *>(self->getObject().get());
	return lws ? lws->shadow.get() : nullptr;
}
template <class T> static void ThreeLightSetShadowMapSize(ThreeWrapper<T> *self, int w, int h)
{
	if (auto *s = ThreeLightShadow(self)) s->mapSize.set((float)w, (float)h);
}
template <class T> static void ThreeLightSetShadowBias(ThreeWrapper<T> *self, float b)
{
	if (auto *s = ThreeLightShadow(self)) s->bias = b;
}
template <class T> static void ThreeLightSetShadowNormalBias(ThreeWrapper<T> *self, float b)
{
	if (auto *s = ThreeLightShadow(self)) s->normalBias = b;
}
template <class T> static void ThreeLightSetShadowRadius(ThreeWrapper<T> *self, float r)
{
	if (auto *s = ThreeLightShadow(self)) s->radius = r;
}
// 影用カメラの near/far (Dir=正投影, Spot/Point=透視 共通)。
template <class T> static void ThreeLightSetShadowNearFar(ThreeWrapper<T> *self, float n, float f)
{
	if (auto *s = ThreeLightShadow(self)) {
		if (s->camera) { s->camera->nearPlane = n; s->camera->farPlane = f; s->camera->updateProjectionMatrix(); }
	}
}
// 平行光源 (DirectionalLight) の影を落とす正投影カメラの範囲。
template <class T> static void ThreeLightSetShadowOrthoBounds(ThreeWrapper<T> *self, float left, float right, float top, float bottom)
{
	if (auto *s = ThreeLightShadow(self)) {
		if (auto *oc = dynamic_cast<threepp::OrthographicCamera *>(s->camera.get())) {
			oc->left = left; oc->right = right; oc->top = top; oc->bottom = bottom;
			oc->updateProjectionMatrix();
		}
	}
}

// LightWithShadow を持つライト (Dir/Spot/Point) の登録ブロックに書くシャドウ設定群。
#define THREE_LIGHT_SHADOW_MEMBERS() \
	NCB_METHOD_PROXY(setShadowMapSize,     ThreeLightSetShadowMapSize<ThreeClass>); \
	NCB_METHOD_PROXY(setShadowBias,        ThreeLightSetShadowBias<ThreeClass>); \
	NCB_METHOD_PROXY(setShadowNormalBias,  ThreeLightSetShadowNormalBias<ThreeClass>); \
	NCB_METHOD_PROXY(setShadowRadius,      ThreeLightSetShadowRadius<ThreeClass>); \
	NCB_METHOD_PROXY(setShadowNearFar,     ThreeLightSetShadowNearFar<ThreeClass>); \
	NCB_METHOD_PROXY(setShadowOrthoBounds, ThreeLightSetShadowOrthoBounds<ThreeClass>)

// 各 Object3D 派生クラスの登録ブロックに書く共通メンバ群。
// type は各クラスが独自の RO プロパティを持つため含めない (衝突回避)。
#define THREE_OBJECT3D_MEMBERS() \
	NCB_THREE_MEMBER_PROPERTY(visible, bool, visible); \
	NCB_THREE_MEMBER_PROPERTY(castShadow, bool, castShadow); \
	NCB_THREE_MEMBER_PROPERTY(receiveShadow, bool, receiveShadow); \
	NCB_THREE_MEMBER_PROPERTY(frustumCulled, bool, frustumCulled); \
	NCB_THREE_MEMBER_PROPERTY(renderOrder, unsigned int, renderOrder); \
	Property(TJS_W("position"),   &ThreeObj3DGet<ThreeClass>::position,   (int)0, getProxyBridgeType(&ThreeWrapper<ThreeClass>::BridgeFunctor::operator())); \
	Property(TJS_W("rotation"),   &ThreeObj3DGet<ThreeClass>::rotation,   (int)0, getProxyBridgeType(&ThreeWrapper<ThreeClass>::BridgeFunctor::operator())); \
	Property(TJS_W("quaternion"), &ThreeObj3DGet<ThreeClass>::quaternion, (int)0, getProxyBridgeType(&ThreeWrapper<ThreeClass>::BridgeFunctor::operator())); \
	Property(TJS_W("scale"),      &ThreeObj3DGet<ThreeClass>::scale,      (int)0, getProxyBridgeType(&ThreeWrapper<ThreeClass>::BridgeFunctor::operator())); \
	Property(TJS_W("up"),         &ThreeObj3DGet<ThreeClass>::up,         (int)0, getProxyBridgeType(&ThreeWrapper<ThreeClass>::BridgeFunctor::operator())); \
	NCB_METHOD_PROXY(add, ThreeObjAdd<ThreeClass>); \
	NCB_METHOD_PROXY(remove, ThreeObjRemove<ThreeClass>); \
	NCB_THREE_METHOD(removeFromParent); \
	NCB_THREE_METHOD(clear); \
	NCB_METHOD_PROXY(lookAt, ThreeObjLookAt<ThreeClass>); \
	NCB_METHOD_PROXY(rotateX, ThreeObjRotateX<ThreeClass>); \
	NCB_METHOD_PROXY(rotateY, ThreeObjRotateY<ThreeClass>); \
	NCB_METHOD_PROXY(rotateZ, ThreeObjRotateZ<ThreeClass>); \
	NCB_METHOD_PROXY(translateX, ThreeObjTranslateX<ThreeClass>); \
	NCB_METHOD_PROXY(translateY, ThreeObjTranslateY<ThreeClass>); \
	NCB_METHOD_PROXY(translateZ, ThreeObjTranslateZ<ThreeClass>); \
	NCB_METHOD_PROXY(setPosition, ThreeObjSetPosition<ThreeClass>); \
	NCB_METHOD_PROXY(setScale, ThreeObjSetScale<ThreeClass>); \
	NCB_METHOD_PROXY(setRotation, ThreeObjSetRotation<ThreeClass>); \
	NCB_METHOD_PROXY(getWorldPosition, ThreeObjGetWorldPosition<ThreeClass>); \
	NCB_METHOD_PROXY(getWorldQuaternion, ThreeObjGetWorldQuaternion<ThreeClass>); \
	NCB_METHOD_PROXY(getWorldScale, ThreeObjGetWorldScale<ThreeClass>); \
	NCB_THREE_METHOD2(updateMatrix, void, updateMatrix, ()); \
	NCB_THREE_METHOD2(updateMatrixWorld, void, updateMatrixWorld, (bool))

NCB_REGISTER_THREE_SUBCLASS(Object3D)
Factory(Object3DFactory);
NCB_METHOD_PROXY(type, Object3DGetType);
NCB_METHOD_PROXY(id, Object3DGetId);
NCB_PROPERTY_PROXY(name, Object3DGetName, Object3DSetName);
// position/rotation/scale/up/visible/add/remove/lookAt/rotate*/translate* 等の
// Object3D 共通メンバ。is-a 派生 (Scene/Group/Camera…) はこれを継承コピーで得る。
THREE_OBJECT3D_MEMBERS();
// マクロに含まれない Object3D 固有の追加メンバ (Vector3 軸指定版)
NCB_METHOD_PROXY(rotateOnAxis, Object3D_rotateOnAxis);
NCB_METHOD_PROXY(rotateOnWorldAxis, Object3D_rotateOnWorldAxis);
NCB_METHOD_PROXY(translateOnAxis, Object3D_translateOnAxis);
};

// ======================================================================
// Group
// ======================================================================

static tjs_error
GroupFactory(ThreeWrapper<Group> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	auto obj = Group::create();
	*result = new ThreeWrapper<Group>(obj);
	return TJS_S_OK;
}

NCB_REGISTER_THREE_SUBCLASS(Group)
Factory(GroupFactory);
NCB_THREE_PROPERTY_RO(type, type);
// Object3D 共通メンバは is-a 継承 (ThreeBaseOf<Group>=Object3D) で自動取得
};

// ======================================================================
// Scene
// ======================================================================

static tjs_error
SceneFactory(ThreeWrapper<Scene> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	auto obj = Scene::create();
	*result = new ThreeWrapper<Scene>(obj);
	return TJS_S_OK;
}

NCB_REGISTER_THREE_SUBCLASS(Scene)
Factory(SceneFactory);
NCB_THREE_PROPERTY_RO(type, type);
NCB_THREE_MEMBER_PROPERTY(autoUpdate, bool, autoUpdate);
// Object3D 共通メンバは is-a 継承 (ThreeBaseOf<Scene>=Object3D) で自動取得
};

// ======================================================================
// Camera (基底)
// ======================================================================

NCB_REGISTER_THREE_SUBCLASS(Camera)
NCB_THREE_PROPERTY_RO(type, type);
NCB_THREE_MEMBER_PROPERTY(zoom, float, zoom);
NCB_THREE_MEMBER_PROPERTY(nearPlane, float, nearPlane);
NCB_THREE_MEMBER_PROPERTY(farPlane, float, farPlane);
NCB_THREE_METHOD(updateProjectionMatrix);
// Object3D 共通メンバは is-a 継承 (ThreeBaseOf<Camera>=Object3D) で自動取得
};

// ======================================================================
// PerspectiveCamera
// ======================================================================

static tjs_error
PerspectiveCameraFactory(ThreeWrapper<PerspectiveCamera> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	float fov = 60.0f;
	float aspect = 1.0f;
	float nearPlane = 0.1f;
	float farPlane = 2000.0f;

	if (numparams > 0) fov = (float)params[0]->AsReal();
	if (numparams > 1) aspect = (float)params[1]->AsReal();
	if (numparams > 2) nearPlane = (float)params[2]->AsReal();
	if (numparams > 3) farPlane = (float)params[3]->AsReal();

	auto obj = PerspectiveCamera::create(fov, aspect, nearPlane, farPlane);
	*result = new ThreeWrapper<PerspectiveCamera>(obj);
	return TJS_S_OK;
}

NCB_REGISTER_THREE_SUBCLASS(PerspectiveCamera)
Factory(PerspectiveCameraFactory);
NCB_THREE_PROPERTY_RO(type, type);
NCB_THREE_MEMBER_PROPERTY(fov, float, fov);
NCB_THREE_MEMBER_PROPERTY(aspect, float, aspect);
NCB_THREE_MEMBER_PROPERTY(focus, float, focus);
NCB_THREE_MEMBER_PROPERTY(filmGauge, float, filmGauge);
NCB_THREE_MEMBER_PROPERTY(filmOffset, float, filmOffset);
NCB_THREE_METHOD(setFocalLength);
NCB_THREE_METHOD(getFocalLength);
NCB_THREE_METHOD(getEffectiveFOV);
NCB_THREE_METHOD(getFilmWidth);
NCB_THREE_METHOD(getFilmHeight);
NCB_THREE_METHOD(setViewOffset);
NCB_THREE_METHOD(clearViewOffset);
NCB_THREE_METHOD(updateProjectionMatrix);
// Object3D 共通メンバは is-a 継承 (Camera 経由で Object3D) で自動取得
};

// ======================================================================
// OrthographicCamera
// ======================================================================

static tjs_error
OrthographicCameraFactory(ThreeWrapper<OrthographicCamera> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	float left = -1.0f;
	float right = 1.0f;
	float top = 1.0f;
	float bottom = -1.0f;
	float nearPlane = 0.1f;
	float farPlane = 2000.0f;

	if (numparams > 0) left = (float)params[0]->AsReal();
	if (numparams > 1) right = (float)params[1]->AsReal();
	if (numparams > 2) top = (float)params[2]->AsReal();
	if (numparams > 3) bottom = (float)params[3]->AsReal();
	if (numparams > 4) nearPlane = (float)params[4]->AsReal();
	if (numparams > 5) farPlane = (float)params[5]->AsReal();

	auto obj = OrthographicCamera::create(left, right, top, bottom, nearPlane, farPlane);
	*result = new ThreeWrapper<OrthographicCamera>(obj);
	return TJS_S_OK;
}

NCB_REGISTER_THREE_SUBCLASS(OrthographicCamera)
Factory(OrthographicCameraFactory);
NCB_THREE_PROPERTY_RO(type, type);
NCB_THREE_MEMBER_PROPERTY(left, float, left);
NCB_THREE_MEMBER_PROPERTY(right, float, right);
NCB_THREE_MEMBER_PROPERTY(top, float, top);
NCB_THREE_MEMBER_PROPERTY(bottom, float, bottom);
NCB_THREE_METHOD(setViewOffset);
NCB_THREE_METHOD(clearViewOffset);
NCB_THREE_METHOD(updateProjectionMatrix);
};

// ======================================================================
// Light (基底)
// ======================================================================

NCB_REGISTER_THREE_SUBCLASS(Light)
NCB_THREE_PROPERTY_RO(type, type);
struct AutoProp_color { static Color ProxyGet(ThreeClass *inst) { return inst->color; } };
Property(TJS_W("color"), &AutoProp_color::ProxyGet, (int)0, getBridgeType(&ThreeWrapper<ThreeClass>::BridgeFunctor::operator()));
NCB_THREE_MEMBER_PROPERTY(intensity, float, intensity);
THREE_OBJECT3D_MEMBERS();
};

// ======================================================================
// AmbientLight
// ======================================================================

static tjs_error
AmbientLightFactory(ThreeWrapper<AmbientLight> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	Color color(0xffffff);
	std::optional<float> intensity = std::nullopt;

	if (numparams > 0) {
		if (params[0]->Type() == tvtInteger) {
			color = Color((unsigned int)params[0]->AsInteger());
		}
	}
	if (numparams > 1) intensity = (float)params[1]->AsReal();

	auto obj = AmbientLight::create(color, intensity);
	*result = new ThreeWrapper<AmbientLight>(obj);
	return TJS_S_OK;
}

NCB_REGISTER_THREE_SUBCLASS(AmbientLight)
Factory(AmbientLightFactory);
NCB_THREE_PROPERTY_RO(type, type);
THREE_OBJECT3D_MEMBERS();
};

// ======================================================================
// DirectionalLight
// ======================================================================

static tjs_error
DirectionalLightFactory(ThreeWrapper<DirectionalLight> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	Color color(0xffffff);
	std::optional<float> intensity = std::nullopt;

	if (numparams > 0) {
		if (params[0]->Type() == tvtInteger) {
			color = Color((unsigned int)params[0]->AsInteger());
		}
	}
	if (numparams > 1) intensity = (float)params[1]->AsReal();

	auto obj = DirectionalLight::create(color, intensity);
	*result = new ThreeWrapper<DirectionalLight>(obj);
	return TJS_S_OK;
}

NCB_REGISTER_THREE_SUBCLASS(DirectionalLight)
Factory(DirectionalLightFactory);
NCB_THREE_PROPERTY_RO(type, type);
NCB_THREE_METHOD(dispose);
THREE_OBJECT3D_MEMBERS();
THREE_LIGHT_SHADOW_MEMBERS();
};

// ======================================================================
// PointLight
// ======================================================================

static tjs_error
PointLightFactory(ThreeWrapper<PointLight> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	Color color(0xffffff);
	std::optional<float> intensity = std::nullopt;
	float distance = 0.0f;
	float decay = 1.0f;

	if (numparams > 0) {
		if (params[0]->Type() == tvtInteger) {
			color = Color((unsigned int)params[0]->AsInteger());
		}
	}
	if (numparams > 1) intensity = (float)params[1]->AsReal();
	if (numparams > 2) distance = (float)params[2]->AsReal();
	if (numparams > 3) decay = (float)params[3]->AsReal();

	auto obj = PointLight::create(color, intensity, distance, decay);
	*result = new ThreeWrapper<PointLight>(obj);
	return TJS_S_OK;
}

NCB_REGISTER_THREE_SUBCLASS(PointLight)
Factory(PointLightFactory);
NCB_THREE_PROPERTY_RO(type, type);
NCB_THREE_MEMBER_PROPERTY(distance, float, distance);
NCB_THREE_MEMBER_PROPERTY(decay, float, decay);
NCB_THREE_METHOD(getPower);
NCB_THREE_METHOD(setPower);
NCB_THREE_METHOD(dispose);
THREE_OBJECT3D_MEMBERS();
THREE_LIGHT_SHADOW_MEMBERS();
};

// ======================================================================
// SpotLight
// ======================================================================

static tjs_error
SpotLightFactory(ThreeWrapper<SpotLight> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	Color color(0xffffff);
	std::optional<float> intensity = std::nullopt;
	float distance = 0.0f;
	float angle = math::PI / 3.0f;
	float penumbra = 0.0f;
	float decay = 1.0f;

	if (numparams > 0) {
		if (params[0]->Type() == tvtInteger) {
			color = Color((unsigned int)params[0]->AsInteger());
		}
	}
	if (numparams > 1) intensity = (float)params[1]->AsReal();
	if (numparams > 2) distance = (float)params[2]->AsReal();
	if (numparams > 3) angle = (float)params[3]->AsReal();
	if (numparams > 4) penumbra = (float)params[4]->AsReal();
	if (numparams > 5) decay = (float)params[5]->AsReal();

	auto obj = SpotLight::create(color, intensity, distance, angle, penumbra, decay);
	*result = new ThreeWrapper<SpotLight>(obj);
	return TJS_S_OK;
}

NCB_REGISTER_THREE_SUBCLASS(SpotLight)
Factory(SpotLightFactory);
NCB_THREE_PROPERTY_RO(type, type);
NCB_THREE_MEMBER_PROPERTY(distance, float, distance);
NCB_THREE_MEMBER_PROPERTY(angle, float, angle);
NCB_THREE_MEMBER_PROPERTY(penumbra, float, penumbra);
NCB_THREE_MEMBER_PROPERTY(decay, float, decay);
NCB_THREE_METHOD(getPower);
NCB_THREE_METHOD(setPower);
NCB_THREE_METHOD(dispose);
THREE_OBJECT3D_MEMBERS();
THREE_LIGHT_SHADOW_MEMBERS();
};

// ======================================================================
// HemisphereLight
// ======================================================================

static tjs_error
HemisphereLightFactory(ThreeWrapper<HemisphereLight> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	Color skyColor(0xffffff);
	Color groundColor(0xffffff);
	std::optional<float> intensity = std::nullopt;

	if (numparams > 0) {
		if (params[0]->Type() == tvtInteger) {
			skyColor = Color((unsigned int)params[0]->AsInteger());
		}
	}
	if (numparams > 1) {
		if (params[1]->Type() == tvtInteger) {
			groundColor = Color((unsigned int)params[1]->AsInteger());
		}
	}
	if (numparams > 2) intensity = (float)params[2]->AsReal();

	auto obj = HemisphereLight::create(skyColor, groundColor, intensity);
	*result = new ThreeWrapper<HemisphereLight>(obj);
	return TJS_S_OK;
}

NCB_REGISTER_THREE_SUBCLASS(HemisphereLight)
Factory(HemisphereLightFactory);
NCB_THREE_PROPERTY_RO(type, type);
struct AutoProp_groundColor { static Color ProxyGet(ThreeClass *inst) { return inst->groundColor; } };
Property(TJS_W("groundColor"), &AutoProp_groundColor::ProxyGet, (int)0, getBridgeType(&ThreeWrapper<ThreeClass>::BridgeFunctor::operator()));
};

// ======================================================================
// Material (基底)
// ======================================================================

NCB_REGISTER_THREE_SUBCLASS(Material)
NCB_THREE_PROPERTY_RO(type, type);
NCB_THREE_MEMBER_PROPERTY_RO(id, unsigned int, id);
NCB_THREE_MEMBER_PROPERTY(name, std::string, name);
NCB_THREE_MEMBER_PROPERTY(fog, bool, fog);
NCB_THREE_MEMBER_PROPERTY(opacity, float, opacity);
NCB_THREE_MEMBER_PROPERTY(transparent, bool, transparent);
NCB_THREE_MEMBER_PROPERTY(depthTest, bool, depthTest);
NCB_THREE_MEMBER_PROPERTY(depthWrite, bool, depthWrite);
NCB_THREE_MEMBER_PROPERTY(colorWrite, bool, colorWrite);
NCB_THREE_MEMBER_PROPERTY(visible, bool, visible);
NCB_THREE_MEMBER_PROPERTY(side, Side, side);
NCB_THREE_MEMBER_PROPERTY(blending, Blending, blending);
NCB_THREE_MEMBER_PROPERTY(alphaTest, float, alphaTest);
NCB_THREE_METHOD(dispose);
NCB_THREE_METHOD(needsUpdate);
};

// ======================================================================
// MeshBasicMaterial
// ======================================================================

static tjs_error
MeshBasicMaterialFactory(ThreeWrapper<MeshBasicMaterial> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	auto obj = MeshBasicMaterial::create();
	*result = new ThreeWrapper<MeshBasicMaterial>(obj);
	return TJS_S_OK;
}

static void MeshBasicMaterialSetColor(ThreeWrapper<MeshBasicMaterial> *self, unsigned int hex)
{
	if (self && self->getObject()) {
		self->getObject()->color.setHex(hex);
	}
}

static unsigned int MeshBasicMaterialGetColor(ThreeWrapper<MeshBasicMaterial> *self)
{
	if (self && self->getObject()) {
		return self->getObject()->color.getHex();
	}
	return 0;
}

// map(カラーテクスチャ)を設定。tex は Texture(DataTexture 等)。is-a により
// shared_ptr<Texture> コンバータが DataTexture の Texture 基底実体を取り出す。
static void MeshBasicMaterialSetMap(ThreeWrapper<MeshBasicMaterial> *self, std::shared_ptr<Texture> tex)
{
	if (self && self->getObject()) self->getObject()->map = tex;
}

NCB_REGISTER_THREE_SUBCLASS(MeshBasicMaterial)
Factory(MeshBasicMaterialFactory);
NCB_THREE_PROPERTY_RO(type, type);
NCB_PROPERTY_PROXY(color, MeshBasicMaterialGetColor, MeshBasicMaterialSetColor);
NCB_METHOD_PROXY(setMap, MeshBasicMaterialSetMap);
NCB_THREE_MEMBER_PROPERTY(wireframe, bool, wireframe);
NCB_THREE_MEMBER_PROPERTY(wireframeLinewidth, float, wireframeLinewidth);
};

// ======================================================================
// MeshLambertMaterial
// ======================================================================

static tjs_error
MeshLambertMaterialFactory(ThreeWrapper<MeshLambertMaterial> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	auto obj = MeshLambertMaterial::create();
	*result = new ThreeWrapper<MeshLambertMaterial>(obj);
	return TJS_S_OK;
}

static void MeshLambertMaterialSetColor(ThreeWrapper<MeshLambertMaterial> *self, unsigned int hex)
{
	if (self && self->getObject()) {
		self->getObject()->color.setHex(hex);
	}
}

static unsigned int MeshLambertMaterialGetColor(ThreeWrapper<MeshLambertMaterial> *self)
{
	if (self && self->getObject()) {
		return self->getObject()->color.getHex();
	}
	return 0;
}

NCB_REGISTER_THREE_SUBCLASS(MeshLambertMaterial)
Factory(MeshLambertMaterialFactory);
NCB_THREE_PROPERTY_RO(type, type);
NCB_PROPERTY_PROXY(color, MeshLambertMaterialGetColor, MeshLambertMaterialSetColor);
NCB_THREE_MEMBER_PROPERTY(wireframe, bool, wireframe);
NCB_THREE_MEMBER_PROPERTY(wireframeLinewidth, float, wireframeLinewidth);
};

// ======================================================================
// MeshPhongMaterial
// ======================================================================

static tjs_error
MeshPhongMaterialFactory(ThreeWrapper<MeshPhongMaterial> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	auto obj = MeshPhongMaterial::create();
	*result = new ThreeWrapper<MeshPhongMaterial>(obj);
	return TJS_S_OK;
}

static void MeshPhongMaterialSetColor(ThreeWrapper<MeshPhongMaterial> *self, unsigned int hex)
{
	if (self && self->getObject()) {
		self->getObject()->color.setHex(hex);
	}
}

static unsigned int MeshPhongMaterialGetColor(ThreeWrapper<MeshPhongMaterial> *self)
{
	if (self && self->getObject()) {
		return self->getObject()->color.getHex();
	}
	return 0;
}

static void MeshPhongMaterialSetEmissive(ThreeWrapper<MeshPhongMaterial> *self, unsigned int hex)
{
	if (self && self->getObject()) {
		self->getObject()->emissive.setHex(hex);
	}
}

static unsigned int MeshPhongMaterialGetEmissive(ThreeWrapper<MeshPhongMaterial> *self)
{
	if (self && self->getObject()) {
		return self->getObject()->emissive.getHex();
	}
	return 0;
}

static void MeshPhongMaterialSetSpecular(ThreeWrapper<MeshPhongMaterial> *self, unsigned int hex)
{
	if (self && self->getObject()) {
		self->getObject()->specular.setHex(hex);
	}
}

static unsigned int MeshPhongMaterialGetSpecular(ThreeWrapper<MeshPhongMaterial> *self)
{
	if (self && self->getObject()) {
		return self->getObject()->specular.getHex();
	}
	return 0;
}

NCB_REGISTER_THREE_SUBCLASS(MeshPhongMaterial)
Factory(MeshPhongMaterialFactory);
NCB_THREE_PROPERTY_RO(type, type);
NCB_PROPERTY_PROXY(color, MeshPhongMaterialGetColor, MeshPhongMaterialSetColor);
NCB_PROPERTY_PROXY(emissive, MeshPhongMaterialGetEmissive, MeshPhongMaterialSetEmissive);
NCB_PROPERTY_PROXY(specular, MeshPhongMaterialGetSpecular, MeshPhongMaterialSetSpecular);
NCB_THREE_MEMBER_PROPERTY(shininess, float, shininess);
NCB_THREE_MEMBER_PROPERTY(wireframe, bool, wireframe);
NCB_THREE_MEMBER_PROPERTY(wireframeLinewidth, float, wireframeLinewidth);
NCB_THREE_MEMBER_PROPERTY(flatShading, bool, flatShading);
};

// ======================================================================
// MeshStandardMaterial
// ======================================================================

static tjs_error
MeshStandardMaterialFactory(ThreeWrapper<MeshStandardMaterial> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	auto obj = MeshStandardMaterial::create();
	*result = new ThreeWrapper<MeshStandardMaterial>(obj);
	return TJS_S_OK;
}

static void MeshStandardMaterialSetColor(ThreeWrapper<MeshStandardMaterial> *self, unsigned int hex)
{
	if (self && self->getObject()) {
		self->getObject()->color.setHex(hex);
	}
}

static unsigned int MeshStandardMaterialGetColor(ThreeWrapper<MeshStandardMaterial> *self)
{
	if (self && self->getObject()) {
		return self->getObject()->color.getHex();
	}
	return 0;
}

static void MeshStandardMaterialSetEmissive(ThreeWrapper<MeshStandardMaterial> *self, unsigned int hex)
{
	if (self && self->getObject()) {
		self->getObject()->emissive.setHex(hex);
	}
}

static unsigned int MeshStandardMaterialGetEmissive(ThreeWrapper<MeshStandardMaterial> *self)
{
	if (self && self->getObject()) {
		return self->getObject()->emissive.getHex();
	}
	return 0;
}

NCB_REGISTER_THREE_SUBCLASS(MeshStandardMaterial)
Factory(MeshStandardMaterialFactory);
NCB_THREE_PROPERTY_RO(type, type);
NCB_PROPERTY_PROXY(color, MeshStandardMaterialGetColor, MeshStandardMaterialSetColor);
NCB_PROPERTY_PROXY(emissive, MeshStandardMaterialGetEmissive, MeshStandardMaterialSetEmissive);
NCB_THREE_MEMBER_PROPERTY(roughness, float, roughness);
NCB_THREE_MEMBER_PROPERTY(metalness, float, metalness);
NCB_THREE_MEMBER_PROPERTY(wireframe, bool, wireframe);
NCB_THREE_MEMBER_PROPERTY(wireframeLinewidth, float, wireframeLinewidth);
NCB_THREE_MEMBER_PROPERTY(flatShading, bool, flatShading);
};

// ======================================================================
// BufferGeometry
// ======================================================================

static tjs_error
BufferGeometryFactory(ThreeWrapper<BufferGeometry> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	auto obj = BufferGeometry::create();
	*result = new ThreeWrapper<BufferGeometry>(obj);
	return TJS_S_OK;
}

// BufferGeometry proxy functions for methods returning BufferGeometry& (chaining) and const std::string& params
static bool BufferGeometry_hasAttribute(ThreeWrapper<BufferGeometry> *self, std::string name) { if (self && self->getObject()) return self->getObject()->hasAttribute(name); return false; }
static void BufferGeometry_rotateX(ThreeWrapper<BufferGeometry> *self, float angle) { if (self && self->getObject()) self->getObject()->rotateX(angle); }
static void BufferGeometry_rotateY(ThreeWrapper<BufferGeometry> *self, float angle) { if (self && self->getObject()) self->getObject()->rotateY(angle); }
static void BufferGeometry_rotateZ(ThreeWrapper<BufferGeometry> *self, float angle) { if (self && self->getObject()) self->getObject()->rotateZ(angle); }
static void BufferGeometry_translate(ThreeWrapper<BufferGeometry> *self, float x, float y, float z) { if (self && self->getObject()) self->getObject()->translate(x, y, z); }
static void BufferGeometry_scale(ThreeWrapper<BufferGeometry> *self, float x, float y, float z) { if (self && self->getObject()) self->getObject()->scale(x, y, z); }
static void BufferGeometry_center(ThreeWrapper<BufferGeometry> *self) { if (self && self->getObject()) self->getObject()->center(); }

NCB_REGISTER_THREE_SUBCLASS(BufferGeometry)
Factory(BufferGeometryFactory);
NCB_THREE_PROPERTY_RO(type, type);
NCB_THREE_MEMBER_PROPERTY_RO(id, unsigned int, id);
NCB_THREE_MEMBER_PROPERTY(name, std::string, name);
NCB_THREE_METHOD(hasIndex);
NCB_METHOD_PROXY(hasAttribute, BufferGeometry_hasAttribute);
NCB_THREE_METHOD(computeBoundingBox);
NCB_THREE_METHOD(computeBoundingSphere);
NCB_THREE_METHOD(computeVertexNormals);
NCB_THREE_METHOD(normalizeNormals);
NCB_METHOD_PROXY(rotateX, BufferGeometry_rotateX);
NCB_METHOD_PROXY(rotateY, BufferGeometry_rotateY);
NCB_METHOD_PROXY(rotateZ, BufferGeometry_rotateZ);
NCB_METHOD_PROXY(translate, BufferGeometry_translate);
NCB_METHOD_PROXY(scale, BufferGeometry_scale);
NCB_METHOD_PROXY(center, BufferGeometry_center);
NCB_THREE_METHOD(dispose);
};

// ======================================================================
// BoxGeometry
// ======================================================================

static tjs_error
BoxGeometryFactory(ThreeWrapper<BoxGeometry> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	float width = 1.0f;
	float height = 1.0f;
	float depth = 1.0f;
	unsigned int widthSegments = 1;
	unsigned int heightSegments = 1;
	unsigned int depthSegments = 1;

	if (numparams > 0) width = (float)params[0]->AsReal();
	if (numparams > 1) height = (float)params[1]->AsReal();
	if (numparams > 2) depth = (float)params[2]->AsReal();
	if (numparams > 3) widthSegments = (unsigned int)params[3]->AsInteger();
	if (numparams > 4) heightSegments = (unsigned int)params[4]->AsInteger();
	if (numparams > 5) depthSegments = (unsigned int)params[5]->AsInteger();

	auto obj = BoxGeometry::create(width, height, depth, widthSegments, heightSegments, depthSegments);
	*result = new ThreeWrapper<BoxGeometry>(obj);
	return TJS_S_OK;
}

NCB_REGISTER_THREE_SUBCLASS(BoxGeometry)
Factory(BoxGeometryFactory);
NCB_THREE_PROPERTY_RO(type, type);
NCB_THREE_MEMBER_PROPERTY_RO(width, float, width);
NCB_THREE_MEMBER_PROPERTY_RO(height, float, height);
NCB_THREE_MEMBER_PROPERTY_RO(depth, float, depth);
};

// ======================================================================
// SphereGeometry
// ======================================================================

static tjs_error
SphereGeometryFactory(ThreeWrapper<SphereGeometry> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	float radius = 1.0f;
	unsigned int widthSegments = 16;
	unsigned int heightSegments = 12;
	float phiStart = 0.0f;
	float phiLength = math::TWO_PI;
	float thetaStart = 0.0f;
	float thetaLength = math::PI;

	if (numparams > 0) radius = (float)params[0]->AsReal();
	if (numparams > 1) widthSegments = (unsigned int)params[1]->AsInteger();
	if (numparams > 2) heightSegments = (unsigned int)params[2]->AsInteger();
	if (numparams > 3) phiStart = (float)params[3]->AsReal();
	if (numparams > 4) phiLength = (float)params[4]->AsReal();
	if (numparams > 5) thetaStart = (float)params[5]->AsReal();
	if (numparams > 6) thetaLength = (float)params[6]->AsReal();

	auto obj = SphereGeometry::create(radius, widthSegments, heightSegments, phiStart, phiLength, thetaStart, thetaLength);
	*result = new ThreeWrapper<SphereGeometry>(obj);
	return TJS_S_OK;
}

NCB_REGISTER_THREE_SUBCLASS(SphereGeometry)
Factory(SphereGeometryFactory);
NCB_THREE_PROPERTY_RO(type, type);
NCB_THREE_MEMBER_PROPERTY_RO(radius, float, radius);
};

// ======================================================================
// PlaneGeometry
// ======================================================================

static tjs_error
PlaneGeometryFactory(ThreeWrapper<PlaneGeometry> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	float width = 1.0f;
	float height = 1.0f;
	unsigned int widthSegments = 1;
	unsigned int heightSegments = 1;

	if (numparams > 0) width = (float)params[0]->AsReal();
	if (numparams > 1) height = (float)params[1]->AsReal();
	if (numparams > 2) widthSegments = (unsigned int)params[2]->AsInteger();
	if (numparams > 3) heightSegments = (unsigned int)params[3]->AsInteger();

	auto obj = PlaneGeometry::create(width, height, widthSegments, heightSegments);
	*result = new ThreeWrapper<PlaneGeometry>(obj);
	return TJS_S_OK;
}

NCB_REGISTER_THREE_SUBCLASS(PlaneGeometry)
Factory(PlaneGeometryFactory);
NCB_THREE_PROPERTY_RO(type, type);
NCB_THREE_MEMBER_PROPERTY_RO(width, float, width);
NCB_THREE_MEMBER_PROPERTY_RO(height, float, height);
NCB_THREE_MEMBER_PROPERTY_RO(widthSegments, unsigned int, widthSegments);
NCB_THREE_MEMBER_PROPERTY_RO(heightSegments, unsigned int, heightSegments);
};

// ======================================================================
// CylinderGeometry
// ======================================================================

static tjs_error
CylinderGeometryFactory(ThreeWrapper<CylinderGeometry> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	float radiusTop = 1.0f;
	float radiusBottom = 1.0f;
	float height = 1.0f;
	unsigned int radialSegments = 8;
	unsigned int heightSegments = 1;
	bool openEnded = false;
	float thetaStart = 0.0f;
	float thetaLength = math::TWO_PI;

	if (numparams > 0) radiusTop = (float)params[0]->AsReal();
	if (numparams > 1) radiusBottom = (float)params[1]->AsReal();
	if (numparams > 2) height = (float)params[2]->AsReal();
	if (numparams > 3) radialSegments = (unsigned int)params[3]->AsInteger();
	if (numparams > 4) heightSegments = (unsigned int)params[4]->AsInteger();
	if (numparams > 5) openEnded = (bool)params[5]->AsInteger();
	if (numparams > 6) thetaStart = (float)params[6]->AsReal();
	if (numparams > 7) thetaLength = (float)params[7]->AsReal();

	auto obj = CylinderGeometry::create(radiusTop, radiusBottom, height, radialSegments, heightSegments, openEnded, thetaStart, thetaLength);
	*result = new ThreeWrapper<CylinderGeometry>(obj);
	return TJS_S_OK;
}

NCB_REGISTER_THREE_SUBCLASS(CylinderGeometry)
Factory(CylinderGeometryFactory);
NCB_THREE_PROPERTY_RO(type, type);
};

// ======================================================================
// ConeGeometry
// ======================================================================

static tjs_error
ConeGeometryFactory(ThreeWrapper<ConeGeometry> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	float radius = 1.0f;
	float height = 1.0f;
	unsigned int radialSegments = 8;
	unsigned int heightSegments = 1;
	bool openEnded = false;
	float thetaStart = 0.0f;
	float thetaLength = math::TWO_PI;

	if (numparams > 0) radius = (float)params[0]->AsReal();
	if (numparams > 1) height = (float)params[1]->AsReal();
	if (numparams > 2) radialSegments = (unsigned int)params[2]->AsInteger();
	if (numparams > 3) heightSegments = (unsigned int)params[3]->AsInteger();
	if (numparams > 4) openEnded = (bool)params[4]->AsInteger();
	if (numparams > 5) thetaStart = (float)params[5]->AsReal();
	if (numparams > 6) thetaLength = (float)params[6]->AsReal();

	auto obj = ConeGeometry::create(radius, height, radialSegments, heightSegments, openEnded, thetaStart, thetaLength);
	*result = new ThreeWrapper<ConeGeometry>(obj);
	return TJS_S_OK;
}

NCB_REGISTER_THREE_SUBCLASS(ConeGeometry)
Factory(ConeGeometryFactory);
NCB_THREE_PROPERTY_RO(type, type);
};

// ======================================================================
// TorusGeometry
// ======================================================================

static tjs_error
TorusGeometryFactory(ThreeWrapper<TorusGeometry> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	float radius = 1.0f;
	float tube = 0.4f;
	unsigned int radialSegments = 8;
	unsigned int tubularSegments = 6;
	float arc = math::TWO_PI;

	if (numparams > 0) radius = (float)params[0]->AsReal();
	if (numparams > 1) tube = (float)params[1]->AsReal();
	if (numparams > 2) radialSegments = (unsigned int)params[2]->AsInteger();
	if (numparams > 3) tubularSegments = (unsigned int)params[3]->AsInteger();
	if (numparams > 4) arc = (float)params[4]->AsReal();

	auto obj = TorusGeometry::create(radius, tube, radialSegments, tubularSegments, arc);
	*result = new ThreeWrapper<TorusGeometry>(obj);
	return TJS_S_OK;
}

NCB_REGISTER_THREE_SUBCLASS(TorusGeometry)
Factory(TorusGeometryFactory);
NCB_THREE_PROPERTY_RO(type, type);
};

// ======================================================================
// TorusKnotGeometry
// ======================================================================

static tjs_error
TorusKnotGeometryFactory(ThreeWrapper<TorusKnotGeometry> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	float radius = 1.0f;
	float tube = 0.4f;
	unsigned int tubularSegments = 64;
	unsigned int radialSegments = 8;
	unsigned int p = 2;
	unsigned int q = 3;

	if (numparams > 0) radius = (float)params[0]->AsReal();
	if (numparams > 1) tube = (float)params[1]->AsReal();
	if (numparams > 2) tubularSegments = (unsigned int)params[2]->AsInteger();
	if (numparams > 3) radialSegments = (unsigned int)params[3]->AsInteger();
	if (numparams > 4) p = (unsigned int)params[4]->AsInteger();
	if (numparams > 5) q = (unsigned int)params[5]->AsInteger();

	auto obj = TorusKnotGeometry::create(radius, tube, tubularSegments, radialSegments, p, q);
	*result = new ThreeWrapper<TorusKnotGeometry>(obj);
	return TJS_S_OK;
}

NCB_REGISTER_THREE_SUBCLASS(TorusKnotGeometry)
Factory(TorusKnotGeometryFactory);
NCB_THREE_PROPERTY_RO(type, type);
};

// ======================================================================
// CircleGeometry
// ======================================================================

static tjs_error
CircleGeometryFactory(ThreeWrapper<CircleGeometry> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	float radius = 1.0f;
	unsigned int segments = 8;
	float thetaStart = 0.0f;
	float thetaLength = math::TWO_PI;

	if (numparams > 0) radius = (float)params[0]->AsReal();
	if (numparams > 1) segments = (unsigned int)params[1]->AsInteger();
	if (numparams > 2) thetaStart = (float)params[2]->AsReal();
	if (numparams > 3) thetaLength = (float)params[3]->AsReal();

	auto obj = CircleGeometry::create(radius, segments, thetaStart, thetaLength);
	*result = new ThreeWrapper<CircleGeometry>(obj);
	return TJS_S_OK;
}

NCB_REGISTER_THREE_SUBCLASS(CircleGeometry)
Factory(CircleGeometryFactory);
NCB_THREE_PROPERTY_RO(type, type);
};

// ======================================================================
// RingGeometry
// ======================================================================

static tjs_error
RingGeometryFactory(ThreeWrapper<RingGeometry> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	float innerRadius = 0.5f;
	float outerRadius = 1.0f;
	unsigned int thetaSegments = 8;
	unsigned int phiSegments = 1;
	float thetaStart = 0.0f;
	float thetaLength = math::TWO_PI;

	if (numparams > 0) innerRadius = (float)params[0]->AsReal();
	if (numparams > 1) outerRadius = (float)params[1]->AsReal();
	if (numparams > 2) thetaSegments = (unsigned int)params[2]->AsInteger();
	if (numparams > 3) phiSegments = (unsigned int)params[3]->AsInteger();
	if (numparams > 4) thetaStart = (float)params[4]->AsReal();
	if (numparams > 5) thetaLength = (float)params[5]->AsReal();

	auto obj = RingGeometry::create(innerRadius, outerRadius, thetaSegments, phiSegments, thetaStart, thetaLength);
	*result = new ThreeWrapper<RingGeometry>(obj);
	return TJS_S_OK;
}

NCB_REGISTER_THREE_SUBCLASS(RingGeometry)
Factory(RingGeometryFactory);
NCB_THREE_PROPERTY_RO(type, type);
};

// ======================================================================
// IcosahedronGeometry
// ======================================================================

static tjs_error
IcosahedronGeometryFactory(ThreeWrapper<IcosahedronGeometry> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	float radius = 1.0f;
	unsigned int detail = 0;

	if (numparams > 0) radius = (float)params[0]->AsReal();
	if (numparams > 1) detail = (unsigned int)params[1]->AsInteger();

	auto obj = IcosahedronGeometry::create(radius, detail);
	*result = new ThreeWrapper<IcosahedronGeometry>(obj);
	return TJS_S_OK;
}

NCB_REGISTER_THREE_SUBCLASS(IcosahedronGeometry)
Factory(IcosahedronGeometryFactory);
NCB_THREE_PROPERTY_RO(type, type);
};

// ======================================================================
// OctahedronGeometry
// ======================================================================

static tjs_error
OctahedronGeometryFactory(ThreeWrapper<OctahedronGeometry> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	float radius = 1.0f;
	unsigned int detail = 0;

	if (numparams > 0) radius = (float)params[0]->AsReal();
	if (numparams > 1) detail = (unsigned int)params[1]->AsInteger();

	auto obj = OctahedronGeometry::create(radius, detail);
	*result = new ThreeWrapper<OctahedronGeometry>(obj);
	return TJS_S_OK;
}

NCB_REGISTER_THREE_SUBCLASS(OctahedronGeometry)
Factory(OctahedronGeometryFactory);
NCB_THREE_PROPERTY_RO(type, type);
};

// ======================================================================
// CapsuleGeometry
// ======================================================================

static tjs_error
CapsuleGeometryFactory(ThreeWrapper<CapsuleGeometry> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	float radius = 0.5f;
	float length = 1.0f;
	unsigned int capSegments = 8;
	unsigned int radialSegments = 16;

	if (numparams > 0) radius = (float)params[0]->AsReal();
	if (numparams > 1) length = (float)params[1]->AsReal();
	if (numparams > 2) capSegments = (unsigned int)params[2]->AsInteger();
	if (numparams > 3) radialSegments = (unsigned int)params[3]->AsInteger();

	auto obj = CapsuleGeometry::create(radius, length, capSegments, radialSegments);
	*result = new ThreeWrapper<CapsuleGeometry>(obj);
	return TJS_S_OK;
}

NCB_REGISTER_THREE_SUBCLASS(CapsuleGeometry)
Factory(CapsuleGeometryFactory);
NCB_THREE_PROPERTY_RO(type, type);
};

// ======================================================================
// LatheGeometry
// ======================================================================

static tjs_error
LatheGeometryFactory(ThreeWrapper<LatheGeometry> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	std::vector<Vector2> points;
	unsigned int segments = 12;
	float phiStart = 0.0f;
	float phiLength = math::TWO_PI;

	// 最初のパラメータは点の配列
	if (numparams > 0 && params[0]->Type() == tvtObject) {
		ncbPropAccessor arr(*params[0]);
		tjs_int count = arr.GetArrayCount();
		for (tjs_int i = 0; i < count; i++) {
			tTJSVariant v;
			if (!arr.checkVariant(i, v)) continue;
			if (v.Type() == tvtObject) {
				ncbPropAccessor pt(v);
				if (IsArray(v)) {
					points.push_back(Vector2((float)pt.getRealValue(0), (float)pt.getRealValue(1)));
				} else {
					points.push_back(Vector2((float)pt.getRealValue(TJS_W("x")), (float)pt.getRealValue(TJS_W("y"))));
				}
			}
		}
	}
	if (numparams > 1) segments = (unsigned int)params[1]->AsInteger();
	if (numparams > 2) phiStart = (float)params[2]->AsReal();
	if (numparams > 3) phiLength = (float)params[3]->AsReal();

	if (points.empty()) {
		// デフォルトの点を追加
		points.push_back(Vector2(0.0f, 0.0f));
		points.push_back(Vector2(0.5f, 0.0f));
		points.push_back(Vector2(0.5f, 1.0f));
		points.push_back(Vector2(0.0f, 1.0f));
	}

	auto obj = LatheGeometry::create(points, segments, phiStart, phiLength);
	*result = new ThreeWrapper<LatheGeometry>(obj);
	return TJS_S_OK;
}

NCB_REGISTER_THREE_SUBCLASS(LatheGeometry)
Factory(LatheGeometryFactory);
NCB_THREE_PROPERTY_RO(type, type);
};

// ======================================================================
// EdgesGeometry
// ======================================================================

static tjs_error
EdgesGeometryFactory(ThreeWrapper<EdgesGeometry> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	BufferGeometry* sourceGeometry = nullptr;
	float thresholdAngle = 1.0f;

	if (numparams > 0 && params[0]->Type() == tvtObject) {
		auto *geoWrapper = ncbInstanceAdaptor<ThreeWrapper<BufferGeometry>>::GetNativeInstance(params[0]->AsObjectNoAddRef());
		if (geoWrapper) sourceGeometry = geoWrapper->getRawPtr();
	}
	if (numparams > 1) thresholdAngle = (float)params[1]->AsReal();

	if (sourceGeometry) {
		auto obj = EdgesGeometry::create(*sourceGeometry, thresholdAngle);
		*result = new ThreeWrapper<EdgesGeometry>(obj);
	} else {
		*result = nullptr;
	}
	return TJS_S_OK;
}

NCB_REGISTER_THREE_SUBCLASS(EdgesGeometry)
Factory(EdgesGeometryFactory);
NCB_THREE_PROPERTY_RO(type, type);
};

// ======================================================================
// WireframeGeometry
// ======================================================================

static tjs_error
WireframeGeometryFactory(ThreeWrapper<WireframeGeometry> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	BufferGeometry* sourceGeometry = nullptr;

	if (numparams > 0 && params[0]->Type() == tvtObject) {
		auto *geoWrapper = ncbInstanceAdaptor<ThreeWrapper<BufferGeometry>>::GetNativeInstance(params[0]->AsObjectNoAddRef());
		if (geoWrapper) sourceGeometry = geoWrapper->getRawPtr();
	}

	if (sourceGeometry) {
		auto obj = WireframeGeometry::create(*sourceGeometry);
		*result = new ThreeWrapper<WireframeGeometry>(obj);
	} else {
		*result = nullptr;
	}
	return TJS_S_OK;
}

NCB_REGISTER_THREE_SUBCLASS(WireframeGeometry)
Factory(WireframeGeometryFactory);
NCB_THREE_PROPERTY_RO(type, type);
};

// ======================================================================
// Mesh
// ======================================================================

static tjs_error
MeshFactory(ThreeWrapper<Mesh> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	std::shared_ptr<BufferGeometry> geometry = nullptr;
	std::shared_ptr<Material> material = nullptr;

	if (numparams > 0 && params[0]->Type() == tvtObject) {
		auto *geoWrapper = ncbInstanceAdaptor<ThreeWrapper<BufferGeometry>>::GetNativeInstance(params[0]->AsObjectNoAddRef());
		if (geoWrapper) geometry = geoWrapper->getObject();
	}
	if (numparams > 1 && params[1]->Type() == tvtObject) {
		auto *matWrapper = ncbInstanceAdaptor<ThreeWrapper<Material>>::GetNativeInstance(params[1]->AsObjectNoAddRef());
		if (matWrapper) material = matWrapper->getObject();
	}

	auto obj = Mesh::create(geometry, material);
	*result = new ThreeWrapper<Mesh>(obj);
	return TJS_S_OK;
}

static void Mesh_setGeometry(ThreeWrapper<Mesh> *self, ThreeWrapper<BufferGeometry> *geo) {
	if (self && self->getObject() && geo && geo->getObject()) self->getObject()->setGeometry(geo->getObject());
}

NCB_REGISTER_THREE_SUBCLASS(Mesh)
Factory(MeshFactory);
NCB_THREE_PROPERTY_RO(type, type);
NCB_METHOD_PROXY(setGeometry, Mesh_setGeometry);
};

// ======================================================================
// Clock
// ======================================================================

static tjs_error
ClockFactory(ThreeWrapper<Clock> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	bool autoStart = true;
	if (numparams > 0) autoStart = (bool)params[0]->AsInteger();

	auto obj = std::make_shared<Clock>(autoStart);
	*result = new ThreeWrapper<Clock>(obj);
	return TJS_S_OK;
}

NCB_REGISTER_THREE_SUBCLASS(Clock)
Factory(ClockFactory);
NCB_THREE_MEMBER_PROPERTY(autoStart, bool, autoStart);
NCB_THREE_MEMBER_PROPERTY_RO(running, bool, running);
NCB_THREE_MEMBER_PROPERTY_RO(elapsedTime, float, elapsedTime);
NCB_THREE_METHOD(start);
NCB_THREE_METHOD(stop);
NCB_THREE_METHOD(getElapsedTime);
NCB_THREE_METHOD(getDelta);
};

// ======================================================================
// GLRenderer
// ======================================================================

// threepp 内部関数の前方宣言 (src/threepp/utils/LoadGlad.hpp は非公開ヘッダのため)。
// ホストが用意した GL コンテキストの getProcAddress を glad に渡すのに使う。
// GLADloadproc は typedef void* (*)(const char*) なので同型で宣言できる。
namespace threepp { void initGlad(void *(*procAddress)(const char *)); }

static tjs_error
GLRendererFactory(ThreeWrapper<GLRenderer> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	int width = 800;
	int height = 600;
	tjs_int sizeArg = 0;

	// 第1引数がオブジェクト (gles の GLESAdaptor 等) なら、そこから GLGetProcAddress を
	// 取得して glad を初期化する。siblings (effekseer/motion) と同じく、ホストが用意した
	// GL コンテキストへ描画するための結線。直後の GLRenderer 構築時に呼ばれる loadGlad が
	// このローダを使って関数ポインタを解決する。
	//   new GLRenderer(adaptor)          … サイズ既定
	//   new GLRenderer(adaptor, w, h)    … サイズ指定
	//   new GLRenderer(w, h)             … 従来どおり (コンテキストと glad は呼び出し側責任)
	if (numparams > 0 && params[0]->Type() == tvtObject) {
		tTJSVariantClosure clo = params[0]->AsObjectClosureNoAddRef();
		tTJSVariant loader_v;
		if (TJS_FAILED(clo.PropGet(0, TJS_W("GLGetProcAddress"), NULL, &loader_v, NULL))) {
			TVPThrowExceptionMessage(TJS_W("GLRenderer: 指定オブジェクトから GLGetProcAddress を取得できません"));
		}
		void *loader = reinterpret_cast<void *>((tjs_int64)loader_v);
		threepp::initGlad(reinterpret_cast<void *(*)(const char *)>(loader));
		sizeArg = 1;
	}

	if (numparams > sizeArg)     width  = (int)params[sizeArg]->AsInteger();
	if (numparams > sizeArg + 1) height = (int)params[sizeArg + 1]->AsInteger();

	auto obj = std::make_shared<GLRenderer>(std::make_pair(width, height));
	*result = new ThreeWrapper<GLRenderer>(obj);
	return TJS_S_OK;
}

// scene は任意の Object3D 派生、camera は任意の Camera 派生を受けられるよう
// tTJSVariant で受けて基底ポインタを取り出す (wrapper 跨ぎ対応)。
static void GLRendererRender(ThreeWrapper<GLRenderer> *self, tTJSVariant sceneV, tTJSVariant cameraV)
{
	if (!self || !self->getObject()) return;
	auto scene = ThreeExtractObject3D(sceneV);
	auto camera = ThreeExtractCamera(cameraV);
	if (scene && camera) self->getObject()->render(*scene, *camera);
}

static void GLRendererSetClearColor(ThreeWrapper<GLRenderer> *self, unsigned int hex, float alpha)
{
	if (self && self->getObject()) {
		self->getObject()->setClearColor(Color(hex), alpha);
	}
}

static void GLRenderer_setSize(ThreeWrapper<GLRenderer> *self, int w, int h) { if (self && self->getObject()) self->getObject()->setSize({w, h}); }

// --- シャドウマップ設定 (renderer.shadowMap は GLShadowMap : ShadowMapConfig) ---
// GLES/捕捉FBO 経路でも動作: シャドウパスは自前 RenderTarget(FBO)へ深度をパッキング描画し、
// 終了時に render target を元(=capture の _defaultFramebuffer)へ復帰する。
static void GLRenderer_setShadowMapEnabled(ThreeWrapper<GLRenderer> *self, bool v)
{
	if (self && self->getObject()) self->getObject()->shadowMap().enabled = v;
}
static void GLRenderer_setShadowMapType(ThreeWrapper<GLRenderer> *self, int type)
{
	if (self && self->getObject()) self->getObject()->shadowMap().type = (ShadowMap)type;
}
static void GLRenderer_setShadowMapAutoUpdate(ThreeWrapper<GLRenderer> *self, bool v)
{
	if (self && self->getObject()) self->getObject()->shadowMap().autoUpdate = v;
}
static void GLRenderer_setShadowMapNeedsUpdate(ThreeWrapper<GLRenderer> *self, bool v)
{
	if (self && self->getObject()) self->getObject()->shadowMap().needsUpdate = v;
}

// --- ホスト GL コンテキストでのフレーム処理 (siblings の onBeginScene/onEndScene 相当) ---
// adaptor.capture() のコールバック内 (GL コンテキストが current な状態) で呼ぶ想定。
//   device.onBeginScene(w, h);
//   device.render(scene, camera);
//   device.onEndScene();
static void GLRendererOnBeginScene(ThreeWrapper<GLRenderer> *self, int w, int h)
{
	if (self && self->getObject()) {
		auto r = self->getObject();
		// ホスト(GLESAdaptor.capture)が捕捉用 FBO を bind した状態でここに入る。
		// beginFrame は sampleCount>1 なら内部 MSAA FBO を bind し(AA描画)、そうでなければ
		// ホストの捕捉 FBO を threepp の画面ターゲットに取り込む。onEndScene の endFrame で
		// MSAA を resolve(blit)してホストの読み戻し先へ書き戻す。
		r->beginFrame(w, h);
		// ホストが GL ステートを変更している可能性があるため、threepp 内部のステート
		// キャッシュを無効化してから描画する (コンテキスト共有対策)。
		r->resetState();
		r->setSize({w, h});
	}
}

static void GLRendererOnEndScene(ThreeWrapper<GLRenderer> *self)
{
	// MSAA 有効時: 内部マルチサンプル FBO をホストの捕捉先 FBO へ resolve(blit)する。
	// 無効時は no-op。
	if (self && self->getObject()) self->getObject()->endFrame();
}

NCB_REGISTER_THREE_SUBCLASS(GLRenderer)
Factory(GLRendererFactory);
NCB_THREE_MEMBER_PROPERTY(autoClear, bool, autoClear);
NCB_THREE_MEMBER_PROPERTY(autoClearColor, bool, autoClearColor);
NCB_THREE_MEMBER_PROPERTY(autoClearDepth, bool, autoClearDepth);
NCB_THREE_MEMBER_PROPERTY(autoClearStencil, bool, autoClearStencil);
NCB_THREE_MEMBER_PROPERTY(sortObjects, bool, sortObjects);
// 本家で GLRenderer::physicallyCorrectLights は廃止され、Renderer::useLegacyLights
// (意味は反転。false=物理的に正しいライティング) に置き換わった。
NCB_THREE_MEMBER_PROPERTY(useLegacyLights, bool, useLegacyLights);
NCB_THREE_MEMBER_PROPERTY(toneMappingExposure, float, toneMappingExposure);
NCB_THREE_METHOD(setPixelRatio);
NCB_METHOD_PROXY(setSize, GLRenderer_setSize);
NCB_THREE_METHOD(setScissorTest);
NCB_THREE_METHOD2(clear, void, clear, (bool, bool, bool));
NCB_THREE_METHOD(clearColor);
NCB_THREE_METHOD(clearDepth);
NCB_THREE_METHOD(clearStencil);
NCB_THREE_METHOD(dispose);
NCB_THREE_METHOD(resetState);
NCB_THREE_METHOD(setSampleCount);  // MSAA サンプル数 (0/1=無効, 既定4)
NCB_METHOD_PROXY(render, GLRendererRender);
NCB_METHOD_PROXY(setClearColor, GLRendererSetClearColor);
NCB_METHOD_PROXY(onBeginScene, GLRendererOnBeginScene);
NCB_METHOD_PROXY(onEndScene, GLRendererOnEndScene);
NCB_METHOD_PROXY(setShadowMapEnabled,    GLRenderer_setShadowMapEnabled);
NCB_METHOD_PROXY(setShadowMapType,       GLRenderer_setShadowMapType);
NCB_METHOD_PROXY(setShadowMapAutoUpdate, GLRenderer_setShadowMapAutoUpdate);
NCB_METHOD_PROXY(setShadowMapNeedsUpdate,GLRenderer_setShadowMapNeedsUpdate);
};

// ======================================================================
// VRM (Phase 0) — vrm/VRM.hpp の VRMLoader / VRM を TJS に公開
// ======================================================================

using krkrvrm::VRM;
using krkrvrm::VRMLoader;
using krkrvrm::VRMAnimation;
using krkrvrm::VRMAnimationLoader;

static tjs_error
VRMLoaderFactory(ThreeWrapper<VRMLoader> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	*result = new ThreeWrapper<VRMLoader>(std::make_shared<VRMLoader>());
	return TJS_S_OK;
}

// load(path) -> VRM（失敗/非VRMは void）。戻り値 shared_ptr は ThreeSharedPtrConvertor で自動変換。
static std::shared_ptr<VRM> VRMLoader_load(ThreeWrapper<VRMLoader> *self, std::string path)
{
	if (self && self->getObject()) return self->getObject()->load(path);
	return nullptr;
}

// 吉里吉里ストレージ(アーカイブ/暗号化/カスタムストレージ対応)からバイト列を読む。
// 失敗時は空 vector。OS パス依存を避け、ホストのストリーム経由でロードするための土台。
static std::vector<unsigned char> ThreeReadStorageBytes(const ttstr &storageName)
{
	std::vector<unsigned char> buf;
	iTJSBinaryStream *st = TVPCreateStream(storageName, TJS_BS_READ);
	if (!st) {
		TVPAddImportantLog(ttstr(TJS_W("[threepp] storage open failed: ")) + storageName);
		return buf;
	}
	tjs_uint64 sz = st->GetSize();
	if (sz > 0) {
		buf.resize((size_t)sz);
		tjs_uint got = st->Read(buf.data(), (tjs_uint)sz);
		if (got != sz) buf.resize(got);
	}
	st->Destruct();  // iTJSBinaryStream は自身を delete する
	return buf;
}

// 吉里吉里ストレージ名(例 "model.vrm")から VRM を読む。アーカイブ内でも可。
static std::shared_ptr<VRM> VRMLoader_loadStorage(ThreeWrapper<VRMLoader> *self, ttstr storageName)
{
	if (!self || !self->getObject()) return nullptr;
	auto bytes = ThreeReadStorageBytes(storageName);
	if (bytes.empty()) return nullptr;
	return self->getObject()->loadFromMemory(bytes.data(), bytes.size());
}

// VRM.scene -> Threepp.Group（Scene に add して render する）
static std::shared_ptr<Group> VRM_getScene(ThreeWrapper<VRM> *self)
{
	return (self && self->getObject()) ? self->getObject()->scene : nullptr;
}

// VRM.getBoneNode(name) -> Threepp.Bone（human bone 名。無ければ void）
static std::shared_ptr<Bone> VRM_getBoneNode(ThreeWrapper<VRM> *self, std::string name)
{
	return (self && self->getObject()) ? self->getObject()->humanoid.getRawBoneNode(name) : nullptr;
}

static int         VRM_getBoneCount(ThreeWrapper<VRM> *self)      { return (self && self->getObject()) ? (int)self->getObject()->humanoid.size() : 0; }

// 表情 (Phase1)。setExpression 後に update() で morph へ反映。
static void  VRM_setExpression(ThreeWrapper<VRM> *self, std::string name, float weight) { if (self && self->getObject()) self->getObject()->expressionManager.setValue(name, weight); }
static float VRM_getExpression(ThreeWrapper<VRM> *self, std::string name)               { return (self && self->getObject()) ? self->getObject()->expressionManager.getValue(name) : 0.f; }
static int   VRM_getExpressionCount(ThreeWrapper<VRM> *self)                             { return (self && self->getObject()) ? (int)self->getObject()->expressionManager.size() : 0; }
static void  VRM_update(ThreeWrapper<VRM> *self, float dt)                                { if (self && self->getObject()) self->getObject()->update(dt); }
static void  VRM_setColliderScale(ThreeWrapper<VRM> *self, float s)                       { if (self && self->getObject()) self->getObject()->setColliderScale(s); }
static float VRM_getColliderScale(ThreeWrapper<VRM> *self)                                { return (self && self->getObject()) ? self->getObject()->getColliderScale() : 1.f; }

// 視線 (Phase1, bone 方式)。setLookAtTarget(ワールド座標) 後 update() で eye ボーンに反映。
static void VRM_setLookAtTarget(ThreeWrapper<VRM> *self, float x, float y, float z) { if (self && self->getObject()) self->getObject()->lookAt.setTarget(x, y, z); }
static void VRM_clearLookAtTarget(ThreeWrapper<VRM> *self)                          { if (self && self->getObject()) self->getObject()->lookAt.clearTarget(); }
static bool VRM_hasLookAt(ThreeWrapper<VRM> *self)                                  { return (self && self->getObject()) ? self->getObject()->lookAt.valid() : false; }

// 正規化 rig (Phase1)。getNormalizedBoneNode で得た Object3D の quaternion にポーズを与え、
// update() (内部で updateRig) で raw ボーンへ転写する。VRMA 適用の土台。
static std::shared_ptr<Object3D> VRM_getNormalizedBoneNode(ThreeWrapper<VRM> *self, std::string name) { return (self && self->getObject()) ? self->getObject()->humanoid.getNormalizedBoneNode(name) : nullptr; }
static void VRM_resetNormalizedPose(ThreeWrapper<VRM> *self) { if (self && self->getObject()) self->getObject()->humanoid.resetNormalizedPose(); }

// VRMA (Phase1)。applyAnimation で時刻 time の姿勢を正規化ボーンへ書き込み、update() で raw へ。
static void VRM_applyAnimation(ThreeWrapper<VRM> *self, ThreeWrapper<VRMAnimation> *anim, float time)
{
	if (self && self->getObject() && anim && anim->getObject())
		self->getObject()->applyAnimation(*anim->getObject(), time);
}

static tjs_error
VRMAnimationLoaderFactory(ThreeWrapper<VRMAnimationLoader> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	*result = new ThreeWrapper<VRMAnimationLoader>(std::make_shared<VRMAnimationLoader>());
	return TJS_S_OK;
}
static std::shared_ptr<VRMAnimation> VRMAnimationLoader_load(ThreeWrapper<VRMAnimationLoader> *self, std::string path)
{
	return (self && self->getObject()) ? self->getObject()->load(path) : nullptr;
}
// 吉里吉里ストレージ名から .vrma を読む。アーカイブ内でも可。
static std::shared_ptr<VRMAnimation> VRMAnimationLoader_loadStorage(ThreeWrapper<VRMAnimationLoader> *self, ttstr storageName)
{
	if (!self || !self->getObject()) return nullptr;
	auto bytes = ThreeReadStorageBytes(storageName);
	if (bytes.empty()) return nullptr;
	return self->getObject()->loadFromMemory(bytes.data(), bytes.size());
}
static float VRMAnimation_getDuration(ThreeWrapper<VRMAnimation> *self)       { return (self && self->getObject()) ? self->getObject()->duration() : 0.f; }
static int   VRMAnimation_getBoneTrackCount(ThreeWrapper<VRMAnimation> *self) { return (self && self->getObject()) ? (int)self->getObject()->boneTrackCount() : 0; }
static std::string VRM_getMetaName(ThreeWrapper<VRM> *self)       { return (self && self->getObject()) ? self->getObject()->meta.name : std::string(); }
static std::string VRM_getMetaVersion(ThreeWrapper<VRM> *self)    { return (self && self->getObject()) ? self->getObject()->meta.version : std::string(); }
static std::string VRM_getMetaLicenseUrl(ThreeWrapper<VRM> *self) { return (self && self->getObject()) ? self->getObject()->meta.licenseUrl : std::string(); }

// VRM / VRMAnimation は VRMLoader/VRMAnimationLoader の戻り値としてコンバータ復路
// (CreateAdaptor) で TJS 化される。エンジンの CreateNew は「未実装コンストラクタ =
// MEMBERNOTFOUND のみ」欠落を許容し、ncbind のダミー(NotImpl)コンストラクタは通らない。
// そこで空の実体を作る Factory を登録する (ncbind の Factory NCM が void センチネルを
// 処理して CreateAdaptor 生成を成功させる。直接 new は空オブジェクトになる)。
static tjs_error VRMFactory(ThreeWrapper<VRM> **result, tjs_int, tTJSVariant **, iTJSDispatch2 *)
{
	*result = new ThreeWrapper<VRM>(std::make_shared<VRM>());
	return TJS_S_OK;
}
static tjs_error VRMAnimationFactory(ThreeWrapper<VRMAnimation> **result, tjs_int, tTJSVariant **, iTJSDispatch2 *)
{
	*result = new ThreeWrapper<VRMAnimation>(std::make_shared<VRMAnimation>());
	return TJS_S_OK;
}

NCB_REGISTER_THREE_SUBCLASS(VRMLoader)
Factory(VRMLoaderFactory);
NCB_METHOD_PROXY(load, VRMLoader_load);              // (OSパス) デバッグ/スタンドアロン用
NCB_METHOD_PROXY(loadStorage, VRMLoader_loadStorage);// (吉里吉里ストレージ名) 本番用
};

NCB_REGISTER_THREE_SUBCLASS(VRM)
Factory(VRMFactory);                                 // コンバータ復路(CreateAdaptor)用
NCB_METHOD_PROXY(getScene, VRM_getScene);            // -> Threepp.Group
NCB_METHOD_PROXY(getBoneNode, VRM_getBoneNode);      // (name) -> Threepp.Bone
NCB_METHOD_PROXY(getBoneCount, VRM_getBoneCount);    // -> int
NCB_METHOD_PROXY(setExpression, VRM_setExpression);  // (name, weight)
NCB_METHOD_PROXY(getExpression, VRM_getExpression);  // (name) -> float
NCB_METHOD_PROXY(getExpressionCount, VRM_getExpressionCount);
NCB_METHOD_PROXY(update, VRM_update);                // morph/視線へ反映 (毎フレーム)
NCB_METHOD_PROXY(setColliderScale, VRM_setColliderScale);// 揺れコライダー半径の一律スケール(既定1.0)
NCB_METHOD_PROXY(getColliderScale, VRM_getColliderScale);
NCB_METHOD_PROXY(setLookAtTarget, VRM_setLookAtTarget);   // (x,y,z) ワールド座標
NCB_METHOD_PROXY(clearLookAtTarget, VRM_clearLookAtTarget);
NCB_METHOD_PROXY(hasLookAt, VRM_hasLookAt);          // -> bool
NCB_METHOD_PROXY(getNormalizedBoneNode, VRM_getNormalizedBoneNode);// (name) -> Object3D
NCB_METHOD_PROXY(resetNormalizedPose, VRM_resetNormalizedPose);
NCB_METHOD_PROXY(applyAnimation, VRM_applyAnimation);// (vrmAnimation, time)
NCB_METHOD_PROXY(getMetaName, VRM_getMetaName);      // -> string
NCB_METHOD_PROXY(getMetaVersion, VRM_getMetaVersion);
NCB_METHOD_PROXY(getMetaLicenseUrl, VRM_getMetaLicenseUrl);
};

NCB_REGISTER_THREE_SUBCLASS(VRMAnimation)
Factory(VRMAnimationFactory);                                   // コンバータ復路(CreateAdaptor)用
NCB_METHOD_PROXY(getDuration, VRMAnimation_getDuration);        // -> float(sec)
NCB_METHOD_PROXY(getBoneTrackCount, VRMAnimation_getBoneTrackCount);
};

NCB_REGISTER_THREE_SUBCLASS(VRMAnimationLoader)
Factory(VRMAnimationLoaderFactory);
NCB_METHOD_PROXY(load, VRMAnimationLoader_load);                // (OSパス) デバッグ用
NCB_METHOD_PROXY(loadStorage, VRMAnimationLoader_loadStorage);  // (吉里吉里ストレージ名) 本番用
};

// ======================================================================
// 汎用 glTF / GLB ローダ (VRM 以外のモデル: 小物/地形/建物/アニメ付きモデル)
// ======================================================================

// GLTFResult を丸ごと保持するホルダ。getScene() で root Group を取り出して
// シーンに add する。animations は将来 AnimationMixer 用に保持しておく。
namespace { struct GLTFModel { threepp::GLTFResult result; }; }

static tjs_error
GLTFLoaderFactory(ThreeWrapper<GLTFLoader> **result, tjs_int, tTJSVariant **, iTJSDispatch2 *)
{
	*result = new ThreeWrapper<GLTFLoader>(std::make_shared<GLTFLoader>());
	return TJS_S_OK;
}

// コンバータ復路(CreateAdaptor)用の空 Factory (VRMFactory と同じ理由)
static tjs_error
GLTFModelFactory(ThreeWrapper<GLTFModel> **result, tjs_int, tTJSVariant **, iTJSDispatch2 *)
{
	*result = new ThreeWrapper<GLTFModel>(std::make_shared<GLTFModel>());
	return TJS_S_OK;
}

// 吉里吉里ストレージ名から glTF/GLB を読む (アーカイブ/暗号化ストレージ内でも可)。
// GLB(自己完結) が確実。外部バッファ/画像を参照する .gltf は basePath 解決なしのため不可。
static std::shared_ptr<GLTFModel> GLTFLoader_loadStorage(ThreeWrapper<GLTFLoader> *self, ttstr storageName)
{
	if (!self || !self->getObject()) return nullptr;
	auto bytes = ThreeReadStorageBytes(storageName);
	if (bytes.empty()) return nullptr;
	auto res = self->getObject()->load(bytes.data(), bytes.size());
	if (!res) return nullptr;
	auto m = std::make_shared<GLTFModel>();
	m->result = std::move(*res);
	return m;
}

// OS パス版 (デバッグ/スタンドアロン用。.gltf + 外部リソースもこちらなら解決可能)
static std::shared_ptr<GLTFModel> GLTFLoader_load(ThreeWrapper<GLTFLoader> *self, std::string path)
{
	if (!self || !self->getObject()) return nullptr;
	auto res = self->getObject()->load(std::filesystem::path(path));
	if (!res) return nullptr;
	auto m = std::make_shared<GLTFModel>();
	m->result = std::move(*res);
	return m;
}

static std::shared_ptr<Group> GLTFModel_getScene(ThreeWrapper<GLTFModel> *self)
{
	return (self && self->getObject()) ? self->getObject()->result.scene : nullptr;
}
static int GLTFModel_getAnimationCount(ThreeWrapper<GLTFModel> *self)
{
	return (self && self->getObject()) ? (int)self->getObject()->result.animations.size() : 0;
}
static std::string GLTFModel_getAnimationName(ThreeWrapper<GLTFModel> *self, int i)
{
	if (self && self->getObject()) {
		auto &anims = self->getObject()->result.animations;
		if (i >= 0 && i < (int)anims.size() && anims[i]) return anims[i]->name();
	}
	return std::string();
}
static float GLTFModel_getAnimationDuration(ThreeWrapper<GLTFModel> *self, int i)
{
	if (self && self->getObject()) {
		auto &anims = self->getObject()->result.animations;
		if (i >= 0 && i < (int)anims.size() && anims[i]) return anims[i]->getDuration();
	}
	return 0.f;
}

NCB_REGISTER_THREE_SUBCLASS(GLTFLoader)
Factory(GLTFLoaderFactory);
NCB_METHOD_PROXY(load, GLTFLoader_load);              // (OSパス) デバッグ/スタンドアロン用
NCB_METHOD_PROXY(loadStorage, GLTFLoader_loadStorage);// (吉里吉里ストレージ名) 本番用 -> GLTFModel
};

NCB_REGISTER_THREE_SUBCLASS(GLTFModel)
Factory(GLTFModelFactory);                            // コンバータ復路(CreateAdaptor)用
NCB_METHOD_PROXY(getScene, GLTFModel_getScene);       // -> Threepp.Group (シーンへ add)
NCB_METHOD_PROXY(getAnimationCount, GLTFModel_getAnimationCount);
NCB_METHOD_PROXY(getAnimationName, GLTFModel_getAnimationName);         // (i) -> string
NCB_METHOD_PROXY(getAnimationDuration, GLTFModel_getAnimationDuration); // (i) -> float(sec)
};

// ======================================================================
// AnimationMixer / AnimationAction (glTF 内蔵アニメ再生)
//
// three.js の AnimationMixer 相当。ただし threepp の AnimationMixer は
// Object3D& を参照で持つため、root を生かし続けるホルダ ModelAnimator 経由で
// 公開する (TJS: Threepp.ModelAnimator)。AnimationAction は mixer 内部所有なので
// ホルダへのエイリアシング shared_ptr で「非所有・ホルダ延命」ラップして返す。
// ======================================================================

namespace {
	struct ModelAnimator {
		std::shared_ptr<Object3D>       root;    // アニメ対象 root (寿命保持)
		std::shared_ptr<AnimationMixer> mixer;
	};
}

// new Threepp.ModelAnimator(rootObject3D)。root は通常 model.getScene()。
static tjs_error
ModelAnimatorFactory(ThreeWrapper<ModelAnimator> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *)
{
	auto h = std::make_shared<ModelAnimator>();
	if (numparams >= 1) {
		auto root = ThreeExtractObject3D(*params[0]);
		if (root) {
			h->root = root;
			h->mixer = std::make_shared<AnimationMixer>(*root);
		}
	}
	*result = new ThreeWrapper<ModelAnimator>(h);
	return TJS_S_OK;
}

// AnimationAction は clipAction の戻り値としてコンバータ復路で TJS 化される。
// AnimationAction はデフォルト構築不可なので、Factory は空ラッパ (obj=null) を作る。
// (実体は復路の new WrapperT(src) 側で設定される。VRMFactory と同じ空実体の役割)
static tjs_error
AnimationActionFactory(ThreeWrapper<AnimationAction> **result, tjs_int, tTJSVariant **, iTJSDispatch2 *)
{
	*result = new ThreeWrapper<AnimationAction>();
	return TJS_S_OK;
}

// AnimationAction* (mixer 内部所有) をホルダへエイリアスして非所有ラップ。
// 返した wrapper が生きている間ホルダ (mixer+root) も生き、action は delete されない。
static std::shared_ptr<AnimationAction> makeActionPtr(const std::shared_ptr<ModelAnimator> &h, AnimationAction *raw)
{
	if (!raw || !h) return nullptr;
	return std::shared_ptr<AnimationAction>(h, raw);
}

// clipAction(model, index) -> AnimationAction (未再生。play() で開始)
static std::shared_ptr<AnimationAction>
ModelAnimator_clipAction(ThreeWrapper<ModelAnimator> *self, ThreeWrapper<GLTFModel> *model, int index)
{
	if (!self || !self->getObject() || !self->getObject()->mixer) return nullptr;
	if (!model || !model->getObject()) return nullptr;
	auto &anims = model->getObject()->result.animations;
	if (index < 0 || index >= (int)anims.size() || !anims[index]) return nullptr;
	AnimationAction *raw = self->getObject()->mixer->clipAction(anims[index]);
	return makeActionPtr(self->getObject(), raw);
}

// play(model, index) -> AnimationAction。ループ設定 + reset + play まで一括 (定番用途)
static std::shared_ptr<AnimationAction>
ModelAnimator_play(ThreeWrapper<ModelAnimator> *self, ThreeWrapper<GLTFModel> *model, int index)
{
	auto a = ModelAnimator_clipAction(self, model, index);
	if (a) { a->setLoop(Loop::Repeat, -1); a->reset(); a->play(); }
	return a;
}

static void ModelAnimator_update(ThreeWrapper<ModelAnimator> *self, float dt)
{ if (self && self->getObject() && self->getObject()->mixer) self->getObject()->mixer->update(dt); }
static void ModelAnimator_stopAll(ThreeWrapper<ModelAnimator> *self)
{ if (self && self->getObject() && self->getObject()->mixer) self->getObject()->mixer->stopAllAction(); }
static void ModelAnimator_setTimeScale(ThreeWrapper<ModelAnimator> *self, float s)
{ if (self && self->getObject() && self->getObject()->mixer) self->getObject()->mixer->timeScale = s; }
static float ModelAnimator_getTimeScale(ThreeWrapper<ModelAnimator> *self)
{ return (self && self->getObject() && self->getObject()->mixer) ? self->getObject()->mixer->timeScale : 1.f; }

// ---- AnimationAction のチェーンメソッド (T& 戻り) は void proxy で公開 ----
static void AnimationAction_play(ThreeWrapper<AnimationAction> *s)  { if (s && s->getObject()) s->getObject()->play(); }
static void AnimationAction_stop(ThreeWrapper<AnimationAction> *s)  { if (s && s->getObject()) s->getObject()->stop(); }
static void AnimationAction_reset(ThreeWrapper<AnimationAction> *s) { if (s && s->getObject()) s->getObject()->reset(); }
static bool AnimationAction_isRunning(ThreeWrapper<AnimationAction> *s) { return (s && s->getObject()) ? s->getObject()->isRunning() : false; }
static void AnimationAction_setLoop(ThreeWrapper<AnimationAction> *s, int mode, int reps) { if (s && s->getObject()) s->getObject()->setLoop((Loop)mode, reps); }
static void AnimationAction_setEffectiveWeight(ThreeWrapper<AnimationAction> *s, float w) { if (s && s->getObject()) s->getObject()->setEffectiveWeight(w); }
static void AnimationAction_setEffectiveTimeScale(ThreeWrapper<AnimationAction> *s, float t) { if (s && s->getObject()) s->getObject()->setEffectiveTimeScale(t); }
static void AnimationAction_fadeIn(ThreeWrapper<AnimationAction> *s, float d)  { if (s && s->getObject()) s->getObject()->fadeIn(d); }
static void AnimationAction_fadeOut(ThreeWrapper<AnimationAction> *s, float d) { if (s && s->getObject()) s->getObject()->fadeOut(d); }
static void AnimationAction_setDuration(ThreeWrapper<AnimationAction> *s, float d) { if (s && s->getObject()) s->getObject()->setDuration(d); }
static void AnimationAction_crossFadeTo(ThreeWrapper<AnimationAction> *s, ThreeWrapper<AnimationAction> *other, float dur, bool warp)
{ if (s && s->getObject() && other && other->getObject()) s->getObject()->crossFadeTo(other->getObject().get(), dur, warp); }

NCB_REGISTER_THREE_SUBCLASS(ModelAnimator)
Factory(ModelAnimatorFactory);
NCB_METHOD_PROXY(clipAction, ModelAnimator_clipAction);   // (model, index) -> AnimationAction
NCB_METHOD_PROXY(play, ModelAnimator_play);               // (model, index) -> AnimationAction (loop+play)
NCB_METHOD_PROXY(update, ModelAnimator_update);           // (dt秒) 毎フレーム呼ぶ
NCB_METHOD_PROXY(stopAll, ModelAnimator_stopAll);
NCB_METHOD_PROXY(setTimeScale, ModelAnimator_setTimeScale);
NCB_METHOD_PROXY(getTimeScale, ModelAnimator_getTimeScale);
};

NCB_REGISTER_THREE_SUBCLASS(AnimationAction)
Factory(AnimationActionFactory);                          // コンバータ復路(CreateAdaptor)用の空実体
NCB_METHOD_PROXY(play, AnimationAction_play);
NCB_METHOD_PROXY(stop, AnimationAction_stop);
NCB_METHOD_PROXY(reset, AnimationAction_reset);
NCB_METHOD_PROXY(isRunning, AnimationAction_isRunning);   // -> bool
NCB_METHOD_PROXY(setLoop, AnimationAction_setLoop);       // (mode:Loop_*, reps) reps=-1で無限
NCB_METHOD_PROXY(setEffectiveWeight, AnimationAction_setEffectiveWeight);
NCB_METHOD_PROXY(setEffectiveTimeScale, AnimationAction_setEffectiveTimeScale);
NCB_METHOD_PROXY(fadeIn, AnimationAction_fadeIn);         // (秒)
NCB_METHOD_PROXY(fadeOut, AnimationAction_fadeOut);       // (秒)
NCB_METHOD_PROXY(crossFadeTo, AnimationAction_crossFadeTo);// (other, dur, warp) でクロスフェード
NCB_METHOD_PROXY(setDuration, AnimationAction_setDuration);
};

// ======================================================================
// Texture
// ======================================================================

static tjs_error
TextureFactory(ThreeWrapper<Texture> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	auto obj = Texture::create();
	*result = new ThreeWrapper<Texture>(obj);
	return TJS_S_OK;
}

NCB_REGISTER_THREE_SUBCLASS(Texture)
Factory(TextureFactory);
NCB_THREE_MEMBER_PROPERTY_RO(id, unsigned int, id);
NCB_THREE_MEMBER_PROPERTY(name, std::string, name);
NCB_THREE_MEMBER_PROPERTY(mapping, Mapping, mapping);
NCB_THREE_MEMBER_PROPERTY(wrapS, TextureWrapping, wrapS);
NCB_THREE_MEMBER_PROPERTY(wrapT, TextureWrapping, wrapT);
NCB_THREE_MEMBER_PROPERTY(magFilter, Filter, magFilter);
NCB_THREE_MEMBER_PROPERTY(minFilter, Filter, minFilter);
NCB_THREE_MEMBER_PROPERTY(anisotropy, int, anisotropy);
NCB_THREE_MEMBER_PROPERTY(format, Format, format);
NCB_THREE_MEMBER_PROPERTY(rotation, float, rotation);
NCB_THREE_MEMBER_PROPERTY(matrixAutoUpdate, bool, matrixAutoUpdate);
NCB_THREE_MEMBER_PROPERTY(generateMipmaps, bool, generateMipmaps);
NCB_THREE_MEMBER_PROPERTY(premultiplyAlpha, bool, premultiplyAlpha);
NCB_THREE_MEMBER_PROPERTY(unpackAlignment, int, unpackAlignment);
// 本家で Texture::encoding(Encoding) は colorSpace(ColorSpace) に置換された。
// TJS 側の名前は互換のため encoding のまま、実体は colorSpace メンバに束縛する。
NCB_THREE_MEMBER_PROPERTY(encoding, ColorSpace, colorSpace);
NCB_THREE_METHOD(updateMatrix);
NCB_THREE_METHOD(dispose);
NCB_THREE_METHOD(needsUpdate);
NCB_THREE_METHOD(version);
};

// ======================================================================
// DataTexture
// ======================================================================

// DataTexture の生成はファクトリで行う (複雑なため簡易実装)
static tjs_error
DataTextureFactory(ThreeWrapper<DataTexture> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	int channels = 4;
	unsigned int width = 1;
	unsigned int height = 1;

	if (numparams > 0) channels = (int)params[0]->AsInteger();
	if (numparams > 1) width = (unsigned int)params[1]->AsInteger();
	if (numparams > 2) height = (unsigned int)params[2]->AsInteger();

	auto obj = DataTexture::create<unsigned char>(channels, width, height);
	*result = new ThreeWrapper<DataTexture>(obj);
	return TJS_S_OK;
}

NCB_REGISTER_THREE_SUBCLASS(DataTexture)
Factory(DataTextureFactory);
};

// 吉里吉里 Layer の画像(32bit ARGB)から threepp テクスチャ(RGBA)を作る。
// Layer は loadImages 等で画像を読み込んでおく。テクスチャ表示の実用パス。
static std::shared_ptr<DataTexture> ThreeTextureFromLayer(tTJSVariant layerV)
{
	if (layerV.Type() != tvtObject) return nullptr;
	iTJSDispatch2 *obj = layerV.AsObjectNoAddRef();
	if (!obj) return nullptr;
	ncbPropAccessor layer(obj);
	tjs_int w     = layer.getIntValue(TJS_W("imageWidth"));
	tjs_int h     = layer.getIntValue(TJS_W("imageHeight"));
	tjs_int pitch = layer.getIntValue(TJS_W("mainImageBufferPitch"));
	const unsigned char *buf = (const unsigned char *)layer.getIntPtrValue(TJS_W("mainImageBufferForWrite"));
	if (!buf || w <= 0 || h <= 0) return nullptr;

	std::vector<unsigned char> data((size_t)w * h * 4);
	for (tjs_int y = 0; y < h; y++) {
		const unsigned char *src = buf + (intptr_t)pitch * y;
		unsigned char *dst = data.data() + (size_t)y * w * 4;
		for (tjs_int x = 0; x < w; x++) {
			// 吉里吉里は 32bit ARGB (メモリ上 B,G,R,A リトルエンディアン) → threepp RGBA
			dst[x * 4 + 0] = src[x * 4 + 2]; // R
			dst[x * 4 + 1] = src[x * 4 + 1]; // G
			dst[x * 4 + 2] = src[x * 4 + 0]; // B
			dst[x * 4 + 3] = src[x * 4 + 3]; // A
		}
	}
	auto tex = DataTexture::create<unsigned char>(4, (unsigned int)w, (unsigned int)h);
	tex->image().setData(ImageData(std::move(data)));
	tex->needsUpdate();
	return tex;
}
NCB_REGISTER_FUNCTION(textureFromLayer, ThreeTextureFromLayer);

// ======================================================================
// TextureLoader
// ======================================================================

class TextureLoaderWrapper {
protected:
	std::unique_ptr<TextureLoader> loader;
public:
	TextureLoaderWrapper(bool useCache = true) : loader(std::make_unique<TextureLoader>(useCache)) {}
	
	ThreeWrapper<Texture>* load(ttstr path, bool flipY = true) {
		std::filesystem::path fsPath(path.c_str());
		auto tex = loader->load(fsPath, flipY);
		if (tex) {
			return new ThreeWrapper<Texture>(tex);
		}
		return nullptr;
	}
	
	void clearCache() {
		loader->clearCache();
	}
};

static tjs_error
TextureLoaderFactory(TextureLoaderWrapper **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	bool useCache = true;
	if (numparams > 0) useCache = (bool)params[0]->AsInteger();
	*result = new TextureLoaderWrapper(useCache);
	return TJS_S_OK;
}

NCB_REGISTER_SUBCLASS(TextureLoaderWrapper) {
	Factory(TextureLoaderFactory);
	NCB_METHOD(load);
	NCB_METHOD(clearCache);
};

// ======================================================================
// Fog
// ======================================================================

NCB_REGISTER_SUBCLASS(Fog) {
	NCB_CONSTRUCTOR((const Color&, float, float));
	NCB_MEMBER_PROPERTY(color, Color, color);
	NCB_MEMBER_PROPERTY(nearPlane, float, nearPlane);
	NCB_MEMBER_PROPERTY(farPlane, float, farPlane);
};

// ======================================================================
// FogExp2
// ======================================================================

NCB_REGISTER_SUBCLASS(FogExp2) {
	NCB_CONSTRUCTOR((const Color&, float));
	NCB_MEMBER_PROPERTY(color, Color, color);
	NCB_MEMBER_PROPERTY(density, float, density);
};

// ======================================================================
// Raycaster
// ======================================================================

// setFromCameraNDC(ndcX, ndcY, camera)
//   ndcX/ndcY は正規化デバイス座標 [-1,1]（three.js の setFromCamera 相当）。
static tjs_error TJS_INTF_METHOD
Raycaster_setFromCameraNDC(tTJSVariant *result, tjs_int numparams, tTJSVariant **param, Raycaster *self)
{
	if (!self) return TJS_E_NATIVECLASSCRASH;
	if (numparams < 3) return TJS_E_BADPARAMCOUNT;
	auto cam = ThreeExtractCamera(*param[2]);
	if (!cam) return TJS_E_INVALIDPARAM;
	float x = (float)(tjs_real)*param[0];
	float y = (float)(tjs_real)*param[1];
	self->setFromCamera(Vector2(x, y), *cam);
	return TJS_S_OK;
}

// setFromCameraPixel(px, py, viewW, viewH, camera)
//   ピクセル座標（左上原点、吉里吉里レイヤ座標）+ ビューポートサイズを NDC に変換して設定。
//   マウス座標をそのまま渡せる利便版。
static tjs_error TJS_INTF_METHOD
Raycaster_setFromCameraPixel(tTJSVariant *result, tjs_int numparams, tTJSVariant **param, Raycaster *self)
{
	if (!self) return TJS_E_NATIVECLASSCRASH;
	if (numparams < 5) return TJS_E_BADPARAMCOUNT;
	auto cam = ThreeExtractCamera(*param[4]);
	if (!cam) return TJS_E_INVALIDPARAM;
	float px = (float)(tjs_real)*param[0];
	float py = (float)(tjs_real)*param[1];
	float vw = (float)(tjs_real)*param[2];
	float vh = (float)(tjs_real)*param[3];
	if (vw <= 0.f || vh <= 0.f) return TJS_E_INVALIDPARAM;
	float ndcX =  (px / vw) * 2.f - 1.f;
	float ndcY = -(py / vh) * 2.f + 1.f;   // Y は上下反転
	self->setFromCamera(Vector2(ndcX, ndcY), *cam);
	return TJS_S_OK;
}

// intersectObjects(objectsArray[, recursive=true]) -> Array
//   結果は距離昇順（[0] が最も手前）。各要素は Dictionary:
//     object   … 引数配列で渡した「ヒットしたルートオブジェクト」（同一 TJS インスタンス）
//     distance … レイ原点からの距離
//     x, y, z  … ワールド座標のヒット点
//   recursive=true のときヒットが子孫でも、引数配列内の祖先ルートに対応付けて返す。
static tjs_error TJS_INTF_METHOD
Raycaster_intersectObjects(tTJSVariant *result, tjs_int numparams, tTJSVariant **param, Raycaster *self)
{
	if (!self) return TJS_E_NATIVECLASSCRASH;
	if (numparams < 1) return TJS_E_BADPARAMCOUNT;
	bool recursive = (numparams >= 2) ? ((tjs_int)*param[1] != 0) : true;

	if (param[0]->Type() != tvtObject) return TJS_E_INVALIDPARAM;
	ncbPropAccessor src(*param[0]);
	tjs_int n = src.GetArrayCount();

	// 引数配列から Object3D* を取り出す（shared_ptr で寿命保持）。
	// 元の TJS variant も保持し、ヒット時にそのまま返す（同一インスタンス識別）。
	std::vector<std::shared_ptr<Object3D>> roots;
	std::vector<tTJSVariant>               rootVars;
	std::vector<Object3D*>                 rawList;
	roots.reserve(n); rootVars.reserve(n); rawList.reserve(n);
	for (tjs_int i = 0; i < n; ++i) {
		tTJSVariant v;
		if (!src.checkVariant(i, v)) continue;
		auto sp = ThreeExtractObject3D(v);
		if (!sp) continue;
		roots.push_back(sp);
		rootVars.push_back(v);
		rawList.push_back(sp.get());
	}

	ncbArrayAccessor out;
	if (!rawList.empty()) {
		auto hits = self->intersectObjects(rawList, recursive);
		tjs_int idx = 0;
		for (const auto &h : hits) {
			// ヒットオブジェクト（子孫かもしれない）を祖先方向に辿り、
			// 引数配列内のルートに対応付ける。
			int ri = -1;
			for (Object3D *o = h.object; o; o = o->parent) {
				for (size_t k = 0; k < rawList.size(); ++k)
					if (rawList[k] == o) { ri = (int)k; break; }
				if (ri >= 0) break;
			}
			ncbDictionaryAccessor d;
			if (ri >= 0) d.SetValue(TJS_W("object"), rootVars[ri]);
			d.SetValue(TJS_W("distance"), (tjs_real)h.distance);
			d.SetValue(TJS_W("x"), (tjs_real)h.point.x);
			d.SetValue(TJS_W("y"), (tjs_real)h.point.y);
			d.SetValue(TJS_W("z"), (tjs_real)h.point.z);
			out.SetValue(idx++, (iTJSDispatch2*)d.GetDispatch());
		}
	}
	if (result) *result = tTJSVariant((iTJSDispatch2*)out.GetDispatch(), (iTJSDispatch2*)out.GetDispatch());
	return TJS_S_OK;
}

NCB_REGISTER_SUBCLASS(Raycaster) {
	NCB_CONSTRUCTOR((const Vector3&, const Vector3&, float, float));
	NCB_MEMBER_PROPERTY(nearPlane, float, nearPlane);
	NCB_MEMBER_PROPERTY(farPlane, float, farPlane);
	NCB_METHOD(set);
	NCB_METHOD_RAW_CALLBACK(setFromCameraNDC,   Raycaster_setFromCameraNDC,   0);
	NCB_METHOD_RAW_CALLBACK(setFromCameraPixel, Raycaster_setFromCameraPixel, 0);
	NCB_METHOD_RAW_CALLBACK(intersectObjects,   Raycaster_intersectObjects,   0);
};

// ======================================================================
// ShaderMaterial
// ======================================================================

static tjs_error
ShaderMaterialFactory(ThreeWrapper<ShaderMaterial> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	auto obj = ShaderMaterial::create();
	*result = new ThreeWrapper<ShaderMaterial>(obj);
	return TJS_S_OK;
}

static void ShaderMaterialSetVertexShader(ThreeWrapper<ShaderMaterial> *self, ttstr shader)
{
	if (self && self->getObject()) {
		const tjs_char* ws = shader.c_str();
		std::string s;
		while (*ws) {
			if (*ws < 128) s += (char)*ws; else s += '?';
			ws++;
		}
		self->getObject()->vertexShader = s;
	}
}

static ttstr ShaderMaterialGetVertexShader(ThreeWrapper<ShaderMaterial> *self)
{
	if (self && self->getObject()) {
		const auto& s = self->getObject()->vertexShader;
		// std::string から ttstr へ変換
		std::wstring ws(s.begin(), s.end());
		return ttstr(ws.c_str());
	}
	return TJS_W("");
}

static void ShaderMaterialSetFragmentShader(ThreeWrapper<ShaderMaterial> *self, ttstr shader)
{
	if (self && self->getObject()) {
		const tjs_char* ws = shader.c_str();
		std::string s;
		while (*ws) {
			if (*ws < 128) {
				s += (char)*ws;
			} else {
				s += '?';
			}
			ws++;
		}
		self->getObject()->fragmentShader = s;
	}
}

static ttstr ShaderMaterialGetFragmentShader(ThreeWrapper<ShaderMaterial> *self)
{
	if (self && self->getObject()) {
		const auto& s = self->getObject()->fragmentShader;
		std::wstring ws(s.begin(), s.end());
		return ttstr(ws.c_str());
	}
	return TJS_W("");
}

NCB_REGISTER_THREE_SUBCLASS(ShaderMaterial)
Factory(ShaderMaterialFactory);
NCB_THREE_PROPERTY_RO(type, type);
NCB_PROPERTY_PROXY(vertexShader, ShaderMaterialGetVertexShader, ShaderMaterialSetVertexShader);
NCB_PROPERTY_PROXY(fragmentShader, ShaderMaterialGetFragmentShader, ShaderMaterialSetFragmentShader);
NCB_THREE_MEMBER_PROPERTY(uniformsNeedUpdate, bool, uniformsNeedUpdate);
NCB_THREE_MEMBER_PROPERTY(wireframe, bool, wireframe);
NCB_THREE_MEMBER_PROPERTY(wireframeLinewidth, float, wireframeLinewidth);
NCB_THREE_MEMBER_PROPERTY(lights, bool, lights);
NCB_THREE_MEMBER_PROPERTY(clipping, bool, clipping);
};

// ======================================================================
// RawShaderMaterial
// ======================================================================

static tjs_error
RawShaderMaterialFactory(ThreeWrapper<RawShaderMaterial> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	auto obj = RawShaderMaterial::create();
	*result = new ThreeWrapper<RawShaderMaterial>(obj);
	return TJS_S_OK;
}

static void RawShaderMaterialSetVertexShader(ThreeWrapper<RawShaderMaterial> *self, ttstr shader)
{
	if (self && self->getObject()) {
		const tjs_char* ws = shader.c_str();
		std::string s;
		while (*ws) {
			if (*ws < 128) {
				s += (char)*ws;
			} else {
				s += '?';
			}
			ws++;
		}
		self->getObject()->vertexShader = s;
	}
}

static ttstr RawShaderMaterialGetVertexShader(ThreeWrapper<RawShaderMaterial> *self)
{
	if (self && self->getObject()) {
		const auto& s = self->getObject()->vertexShader;
		std::wstring ws(s.begin(), s.end());
		return ttstr(ws.c_str());
	}
	return TJS_W("");
}

static void RawShaderMaterialSetFragmentShader(ThreeWrapper<RawShaderMaterial> *self, ttstr shader)
{
	if (self && self->getObject()) {
		const tjs_char* ws = shader.c_str();
		std::string s;
		while (*ws) {
			if (*ws < 128) {
				s += (char)*ws;
			} else {
				s += '?';
			}
			ws++;
		}
		self->getObject()->fragmentShader = s;
	}
}

static ttstr RawShaderMaterialGetFragmentShader(ThreeWrapper<RawShaderMaterial> *self)
{
	if (self && self->getObject()) {
		const auto& s = self->getObject()->fragmentShader;
		std::wstring ws(s.begin(), s.end());
		return ttstr(ws.c_str());
	}
	return TJS_W("");
}

NCB_REGISTER_THREE_SUBCLASS(RawShaderMaterial)
Factory(RawShaderMaterialFactory);

NCB_PROPERTY_PROXY(vertexShader, RawShaderMaterialGetVertexShader, RawShaderMaterialSetVertexShader);
NCB_PROPERTY_PROXY(fragmentShader, RawShaderMaterialGetFragmentShader, RawShaderMaterialSetFragmentShader);
NCB_THREE_MEMBER_PROPERTY(uniformsNeedUpdate, bool, uniformsNeedUpdate);
};

// ======================================================================
// LineBasicMaterial
// ======================================================================

static tjs_error
LineBasicMaterialFactory(ThreeWrapper<LineBasicMaterial> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	auto obj = LineBasicMaterial::create();
	*result = new ThreeWrapper<LineBasicMaterial>(obj);
	return TJS_S_OK;
}

static void LineBasicMaterialSetColor(ThreeWrapper<LineBasicMaterial> *self, unsigned int hex)
{
	if (self && self->getObject()) {
		self->getObject()->color.setHex(hex);
	}
}

static unsigned int LineBasicMaterialGetColor(ThreeWrapper<LineBasicMaterial> *self)
{
	if (self && self->getObject()) {
		return self->getObject()->color.getHex();
	}
	return 0;
}

NCB_REGISTER_THREE_SUBCLASS(LineBasicMaterial)
Factory(LineBasicMaterialFactory);

NCB_PROPERTY_PROXY(color, LineBasicMaterialGetColor, LineBasicMaterialSetColor);
NCB_THREE_MEMBER_PROPERTY(linewidth, float, linewidth);
};

// ======================================================================
// PointsMaterial
// ======================================================================

static tjs_error
PointsMaterialFactory(ThreeWrapper<PointsMaterial> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	auto obj = PointsMaterial::create();
	*result = new ThreeWrapper<PointsMaterial>(obj);
	return TJS_S_OK;
}

static void PointsMaterialSetColor(ThreeWrapper<PointsMaterial> *self, unsigned int hex)
{
	if (self && self->getObject()) {
		self->getObject()->color.setHex(hex);
	}
}

static unsigned int PointsMaterialGetColor(ThreeWrapper<PointsMaterial> *self)
{
	if (self && self->getObject()) {
		return self->getObject()->color.getHex();
	}
	return 0;
}

NCB_REGISTER_THREE_SUBCLASS(PointsMaterial)
Factory(PointsMaterialFactory);
NCB_THREE_PROPERTY_RO(type, type);
NCB_PROPERTY_PROXY(color, PointsMaterialGetColor, PointsMaterialSetColor);
NCB_THREE_MEMBER_PROPERTY(size, float, size);
NCB_THREE_MEMBER_PROPERTY(sizeAttenuation, bool, sizeAttenuation);
};

// ======================================================================
// SpriteMaterial
// ======================================================================

static tjs_error
SpriteMaterialFactory(ThreeWrapper<SpriteMaterial> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	auto obj = SpriteMaterial::create();
	*result = new ThreeWrapper<SpriteMaterial>(obj);
	return TJS_S_OK;
}

static void SpriteMaterialSetColor(ThreeWrapper<SpriteMaterial> *self, unsigned int hex)
{
	if (self && self->getObject()) {
		self->getObject()->color.setHex(hex);
	}
}

static unsigned int SpriteMaterialGetColor(ThreeWrapper<SpriteMaterial> *self)
{
	if (self && self->getObject()) {
		return self->getObject()->color.getHex();
	}
	return 0;
}

NCB_REGISTER_THREE_SUBCLASS(SpriteMaterial)
Factory(SpriteMaterialFactory);
NCB_THREE_PROPERTY_RO(type, type);
NCB_PROPERTY_PROXY(color, SpriteMaterialGetColor, SpriteMaterialSetColor);
NCB_THREE_MEMBER_PROPERTY(rotation, float, rotation);
NCB_THREE_MEMBER_PROPERTY(sizeAttenuation, bool, sizeAttenuation);
};

// ======================================================================
// MeshNormalMaterial
// ======================================================================

static tjs_error
MeshNormalMaterialFactory(ThreeWrapper<MeshNormalMaterial> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	auto obj = MeshNormalMaterial::create();
	*result = new ThreeWrapper<MeshNormalMaterial>(obj);
	return TJS_S_OK;
}

NCB_REGISTER_THREE_SUBCLASS(MeshNormalMaterial)
Factory(MeshNormalMaterialFactory);
NCB_THREE_PROPERTY_RO(type, type);
NCB_THREE_MEMBER_PROPERTY(wireframe, bool, wireframe);
NCB_THREE_MEMBER_PROPERTY(wireframeLinewidth, float, wireframeLinewidth);
NCB_THREE_MEMBER_PROPERTY(flatShading, bool, flatShading);
};

// ======================================================================
// MeshDepthMaterial
// ======================================================================

static tjs_error
MeshDepthMaterialFactory(ThreeWrapper<MeshDepthMaterial> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	auto obj = MeshDepthMaterial::create();
	*result = new ThreeWrapper<MeshDepthMaterial>(obj);
	return TJS_S_OK;
}

NCB_REGISTER_THREE_SUBCLASS(MeshDepthMaterial)
Factory(MeshDepthMaterialFactory);
NCB_THREE_PROPERTY_RO(type, type);
NCB_THREE_MEMBER_PROPERTY(wireframe, bool, wireframe);
NCB_THREE_MEMBER_PROPERTY(wireframeLinewidth, float, wireframeLinewidth);
};

// ======================================================================
// MeshToonMaterial
// ======================================================================

static tjs_error
MeshToonMaterialFactory(ThreeWrapper<MeshToonMaterial> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	auto obj = MeshToonMaterial::create();
	*result = new ThreeWrapper<MeshToonMaterial>(obj);
	return TJS_S_OK;
}

static void MeshToonMaterialSetColor(ThreeWrapper<MeshToonMaterial> *self, unsigned int hex)
{
	if (self && self->getObject()) {
		self->getObject()->color.setHex(hex);
	}
}

static unsigned int MeshToonMaterialGetColor(ThreeWrapper<MeshToonMaterial> *self)
{
	if (self && self->getObject()) {
		return self->getObject()->color.getHex();
	}
	return 0;
}

static void MeshToonMaterialSetEmissive(ThreeWrapper<MeshToonMaterial> *self, unsigned int hex)
{
	if (self && self->getObject()) {
		self->getObject()->emissive.setHex(hex);
	}
}

static unsigned int MeshToonMaterialGetEmissive(ThreeWrapper<MeshToonMaterial> *self)
{
	if (self && self->getObject()) {
		return self->getObject()->emissive.getHex();
	}
	return 0;
}

NCB_REGISTER_THREE_SUBCLASS(MeshToonMaterial)
Factory(MeshToonMaterialFactory);
NCB_THREE_PROPERTY_RO(type, type);
NCB_PROPERTY_PROXY(color, MeshToonMaterialGetColor, MeshToonMaterialSetColor);
NCB_PROPERTY_PROXY(emissive, MeshToonMaterialGetEmissive, MeshToonMaterialSetEmissive);
NCB_THREE_MEMBER_PROPERTY(wireframe, bool, wireframe);
NCB_THREE_MEMBER_PROPERTY(wireframeLinewidth, float, wireframeLinewidth);
};

// ======================================================================
// MeshMatcapMaterial
// ======================================================================

static tjs_error
MeshMatcapMaterialFactory(ThreeWrapper<MeshMatcapMaterial> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	auto obj = MeshMatcapMaterial::create();
	*result = new ThreeWrapper<MeshMatcapMaterial>(obj);
	return TJS_S_OK;
}

static void MeshMatcapMaterialSetColor(ThreeWrapper<MeshMatcapMaterial> *self, unsigned int hex)
{
	if (self && self->getObject()) {
		self->getObject()->color.setHex(hex);
	}
}

static unsigned int MeshMatcapMaterialGetColor(ThreeWrapper<MeshMatcapMaterial> *self)
{
	if (self && self->getObject()) {
		return self->getObject()->color.getHex();
	}
	return 0;
}

NCB_REGISTER_THREE_SUBCLASS(MeshMatcapMaterial)
Factory(MeshMatcapMaterialFactory);
NCB_THREE_PROPERTY_RO(type, type);
NCB_PROPERTY_PROXY(color, MeshMatcapMaterialGetColor, MeshMatcapMaterialSetColor);
NCB_THREE_MEMBER_PROPERTY(flatShading, bool, flatShading);
};

// ======================================================================
// Line
// ======================================================================

static tjs_error
LineFactory(ThreeWrapper<Line> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	std::shared_ptr<BufferGeometry> geometry = nullptr;
	std::shared_ptr<Material> material = nullptr;

	if (numparams > 0 && params[0]->Type() == tvtObject) {
		auto *geoWrapper = ncbInstanceAdaptor<ThreeWrapper<BufferGeometry>>::GetNativeInstance(params[0]->AsObjectNoAddRef());
		if (geoWrapper) geometry = geoWrapper->getObject();
	}
	if (numparams > 1 && params[1]->Type() == tvtObject) {
		auto *matWrapper = ncbInstanceAdaptor<ThreeWrapper<Material>>::GetNativeInstance(params[1]->AsObjectNoAddRef());
		if (matWrapper) material = matWrapper->getObject();
	}

	auto obj = Line::create(geometry, material);
	*result = new ThreeWrapper<Line>(obj);
	return TJS_S_OK;
}

static void Line_setGeometry(ThreeWrapper<Line> *self, ThreeWrapper<BufferGeometry> *geo) {
	if (self && self->getObject() && geo && geo->getObject()) self->getObject()->setGeometry(geo->getObject());
}

NCB_REGISTER_THREE_SUBCLASS(Line)
Factory(LineFactory);
NCB_THREE_PROPERTY_RO(type, type);
NCB_THREE_METHOD(computeLineDistances);
NCB_METHOD_PROXY(setGeometry, Line_setGeometry);
};

// ======================================================================
// LineSegments
// ======================================================================

static tjs_error
LineSegmentsFactory(ThreeWrapper<LineSegments> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	std::shared_ptr<BufferGeometry> geometry = nullptr;
	std::shared_ptr<Material> material = nullptr;

	if (numparams > 0 && params[0]->Type() == tvtObject) {
		auto *geoWrapper = ncbInstanceAdaptor<ThreeWrapper<BufferGeometry>>::GetNativeInstance(params[0]->AsObjectNoAddRef());
		if (geoWrapper) geometry = geoWrapper->getObject();
	}
	if (numparams > 1 && params[1]->Type() == tvtObject) {
		auto *matWrapper = ncbInstanceAdaptor<ThreeWrapper<Material>>::GetNativeInstance(params[1]->AsObjectNoAddRef());
		if (matWrapper) material = matWrapper->getObject();
	}

	auto obj = LineSegments::create(geometry, material);
	*result = new ThreeWrapper<LineSegments>(obj);
	return TJS_S_OK;
}

NCB_REGISTER_THREE_SUBCLASS(LineSegments)
Factory(LineSegmentsFactory);
NCB_THREE_PROPERTY_RO(type, type);
};

// ======================================================================
// Points
// ======================================================================

static tjs_error
PointsFactory(ThreeWrapper<Points> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	std::shared_ptr<BufferGeometry> geometry = BufferGeometry::create();
	std::shared_ptr<Material> material = PointsMaterial::create();

	if (numparams > 0 && params[0]->Type() == tvtObject) {
		auto *geoWrapper = ncbInstanceAdaptor<ThreeWrapper<BufferGeometry>>::GetNativeInstance(params[0]->AsObjectNoAddRef());
		if (geoWrapper) geometry = geoWrapper->getObject();
	}
	if (numparams > 1 && params[1]->Type() == tvtObject) {
		auto *matWrapper = ncbInstanceAdaptor<ThreeWrapper<Material>>::GetNativeInstance(params[1]->AsObjectNoAddRef());
		if (matWrapper) material = matWrapper->getObject();
	}

	auto obj = Points::create(geometry, material);
	*result = new ThreeWrapper<Points>(obj);
	return TJS_S_OK;
}

static void Points_setGeometry(ThreeWrapper<Points> *self, ThreeWrapper<BufferGeometry> *geo) {
	if (self && self->getObject() && geo && geo->getObject()) self->getObject()->setGeometry(geo->getObject());
}

NCB_REGISTER_THREE_SUBCLASS(Points)
Factory(PointsFactory);
NCB_THREE_PROPERTY_RO(type, type);
NCB_METHOD_PROXY(setGeometry, Points_setGeometry);
};

// ======================================================================
// Sprite
// ======================================================================

static tjs_error
SpriteFactory(ThreeWrapper<Sprite> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	std::shared_ptr<SpriteMaterial> material = nullptr;

	if (numparams > 0 && params[0]->Type() == tvtObject) {
		auto *matWrapper = ncbInstanceAdaptor<ThreeWrapper<SpriteMaterial>>::GetNativeInstance(params[0]->AsObjectNoAddRef());
		if (matWrapper) material = matWrapper->getObject();
	}

	auto obj = Sprite::create(material);
	*result = new ThreeWrapper<Sprite>(obj);
	return TJS_S_OK;
}

NCB_REGISTER_THREE_SUBCLASS(Sprite)
Factory(SpriteFactory);
NCB_THREE_PROPERTY_RO(type, type);
struct AutoProp_center { static Vector2 ProxyGet(ThreeClass *inst) { return inst->center; } };
Property(TJS_W("center"), &AutoProp_center::ProxyGet, (int)0, getBridgeType(&ThreeWrapper<ThreeClass>::BridgeFunctor::operator()));
};

// ======================================================================
// InstancedMesh
// ======================================================================

static tjs_error
InstancedMeshFactory(ThreeWrapper<InstancedMesh> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	std::shared_ptr<BufferGeometry> geometry = nullptr;
	std::shared_ptr<Material> material = nullptr;
	size_t count = 1;

	if (numparams > 0 && params[0]->Type() == tvtObject) {
		auto *geoWrapper = ncbInstanceAdaptor<ThreeWrapper<BufferGeometry>>::GetNativeInstance(params[0]->AsObjectNoAddRef());
		if (geoWrapper) geometry = geoWrapper->getObject();
	}
	if (numparams > 1 && params[1]->Type() == tvtObject) {
		auto *matWrapper = ncbInstanceAdaptor<ThreeWrapper<Material>>::GetNativeInstance(params[1]->AsObjectNoAddRef());
		if (matWrapper) material = matWrapper->getObject();
	}
	if (numparams > 2) count = (size_t)params[2]->AsInteger();

	auto obj = InstancedMesh::create(geometry, material, count);
	*result = new ThreeWrapper<InstancedMesh>(obj);
	return TJS_S_OK;
}

static size_t InstancedMeshGetCount(ThreeWrapper<InstancedMesh> *self)
{
	if (self && self->getObject()) {
		return self->getObject()->count();
	}
	return 0;
}

static void InstancedMeshSetCount(ThreeWrapper<InstancedMesh> *self, size_t count)
{
	if (self && self->getObject()) {
		self->getObject()->setCount(count);
	}
}

NCB_REGISTER_THREE_SUBCLASS(InstancedMesh)
Factory(InstancedMeshFactory);
NCB_THREE_PROPERTY_RO(type, type);
NCB_PROPERTY_PROXY(count, InstancedMeshGetCount, InstancedMeshSetCount);
NCB_THREE_METHOD(getMatrixAt);
NCB_THREE_METHOD(setMatrixAt);
NCB_THREE_METHOD(getColorAt);
NCB_THREE_METHOD(setColorAt);
NCB_THREE_METHOD(computeBoundingBox);
NCB_THREE_METHOD(computeBoundingSphere);
NCB_THREE_METHOD(dispose);
};

// ======================================================================
// AxesHelper
// ======================================================================

static tjs_error
AxesHelperFactory(ThreeWrapper<AxesHelper> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	float size = 1.0f;
	if (numparams > 0) size = (float)params[0]->AsReal();

	auto obj = AxesHelper::create(size);
	*result = new ThreeWrapper<AxesHelper>(obj);
	return TJS_S_OK;
}

NCB_REGISTER_THREE_SUBCLASS(AxesHelper)
Factory(AxesHelperFactory);

};

// ======================================================================
// GridHelper
// ======================================================================

static tjs_error
GridHelperFactory(ThreeWrapper<GridHelper> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	unsigned int size = 10;
	unsigned int divisions = 10;
	Color color1(0x444444);
	Color color2(0x888888);

	if (numparams > 0) size = (unsigned int)params[0]->AsInteger();
	if (numparams > 1) divisions = (unsigned int)params[1]->AsInteger();
	if (numparams > 2) color1 = Color((unsigned int)params[2]->AsInteger());
	if (numparams > 3) color2 = Color((unsigned int)params[3]->AsInteger());

	auto obj = GridHelper::create(size, divisions, color1, color2);
	*result = new ThreeWrapper<GridHelper>(obj);
	return TJS_S_OK;
}

NCB_REGISTER_THREE_SUBCLASS(GridHelper)
Factory(GridHelperFactory);

};

// ======================================================================
// ArrowHelper
// ======================================================================

static tjs_error
ArrowHelperFactory(ThreeWrapper<ArrowHelper> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	Vector3 dir(0, 0, 1);
	Vector3 origin(0, 0, 0);
	float length = 1.0f;
	Color color(0xffff00);

	if (numparams > 0 && params[0]->Type() == tvtObject) {
		ncbPropAccessor info(*params[0]);
		if (IsArray(*params[0])) {
			dir = Vector3((float)info.getRealValue(0), (float)info.getRealValue(1), (float)info.getRealValue(2));
		}
	}
	if (numparams > 1 && params[1]->Type() == tvtObject) {
		ncbPropAccessor info(*params[1]);
		if (IsArray(*params[1])) {
			origin = Vector3((float)info.getRealValue(0), (float)info.getRealValue(1), (float)info.getRealValue(2));
		}
	}
	if (numparams > 2) length = (float)params[2]->AsReal();
	if (numparams > 3) color = Color((unsigned int)params[3]->AsInteger());

	auto obj = ArrowHelper::create(dir, origin, length, color);
	*result = new ThreeWrapper<ArrowHelper>(obj);
	return TJS_S_OK;
}

static void ArrowHelper_setLength(ThreeWrapper<ArrowHelper> *self, float length) { if (self && self->getObject()) self->getObject()->setLength(length); }

NCB_REGISTER_THREE_SUBCLASS(ArrowHelper)
Factory(ArrowHelperFactory);
NCB_THREE_METHOD(setDirection);
NCB_METHOD_PROXY(setLength, ArrowHelper_setLength);
NCB_THREE_METHOD(setColor);
};

// ======================================================================
// BoxHelper
// ======================================================================

// BoxHelper は Object3D への参照が必要なため、簡易的なファクトリを用意
static tjs_error
BoxHelperFactory(ThreeWrapper<BoxHelper> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	Color color(0xffff00);
	Object3D* targetObj = nullptr;

	if (numparams > 0 && params[0]->Type() == tvtObject) {
		auto *objWrapper = ncbInstanceAdaptor<ThreeWrapper<Object3D>>::GetNativeInstance(params[0]->AsObjectNoAddRef());
		if (objWrapper) targetObj = objWrapper->getRawPtr();
	}
	if (numparams > 1) color = Color((unsigned int)params[1]->AsInteger());

	if (targetObj) {
		auto obj = BoxHelper::create(*targetObj, color);
		*result = new ThreeWrapper<BoxHelper>(obj);
	} else {
		*result = nullptr;
	}
	return TJS_S_OK;
}

NCB_REGISTER_THREE_SUBCLASS(BoxHelper)
Factory(BoxHelperFactory);

NCB_THREE_METHOD(update);
};

// ======================================================================
// CameraHelper
// ======================================================================

static tjs_error
CameraHelperFactory(ThreeWrapper<CameraHelper> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	Camera* camera = nullptr;

	if (numparams > 0 && params[0]->Type() == tvtObject) {
		auto *camWrapper = ncbInstanceAdaptor<ThreeWrapper<Camera>>::GetNativeInstance(params[0]->AsObjectNoAddRef());
		if (camWrapper) camera = camWrapper->getRawPtr();
	}

	if (camera) {
		auto obj = CameraHelper::create(*camera);
		*result = new ThreeWrapper<CameraHelper>(obj);
	} else {
		*result = nullptr;
	}
	return TJS_S_OK;
}

NCB_REGISTER_THREE_SUBCLASS(CameraHelper)
Factory(CameraHelperFactory);

NCB_THREE_METHOD(update);
};

// ======================================================================
// DirectionalLightHelper
// ======================================================================

static tjs_error
DirectionalLightHelperFactory(ThreeWrapper<DirectionalLightHelper> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	DirectionalLight* light = nullptr;
	float size = 1.0f;
	std::optional<Color> color = std::nullopt;

	if (numparams > 0 && params[0]->Type() == tvtObject) {
		auto *lightWrapper = ncbInstanceAdaptor<ThreeWrapper<DirectionalLight>>::GetNativeInstance(params[0]->AsObjectNoAddRef());
		if (lightWrapper) light = lightWrapper->getRawPtr();
	}
	if (numparams > 1) size = (float)params[1]->AsReal();
	if (numparams > 2) color = Color((unsigned int)params[2]->AsInteger());

	if (light) {
		auto obj = DirectionalLightHelper::create(*light, size, color);
		*result = new ThreeWrapper<DirectionalLightHelper>(obj);
	} else {
		*result = nullptr;
	}
	return TJS_S_OK;
}

NCB_REGISTER_THREE_SUBCLASS(DirectionalLightHelper)
Factory(DirectionalLightHelperFactory);

NCB_THREE_METHOD(update);
};

// ======================================================================
// PointLightHelper
// ======================================================================

static tjs_error
PointLightHelperFactory(ThreeWrapper<PointLightHelper> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	PointLight* light = nullptr;
	float sphereSize = 1.0f;
	std::optional<Color> color = std::nullopt;

	if (numparams > 0 && params[0]->Type() == tvtObject) {
		auto *lightWrapper = ncbInstanceAdaptor<ThreeWrapper<PointLight>>::GetNativeInstance(params[0]->AsObjectNoAddRef());
		if (lightWrapper) light = lightWrapper->getRawPtr();
	}
	if (numparams > 1) sphereSize = (float)params[1]->AsReal();
	if (numparams > 2) color = Color((unsigned int)params[2]->AsInteger());

	if (light) {
		auto obj = PointLightHelper::create(*light, sphereSize, color);
		*result = new ThreeWrapper<PointLightHelper>(obj);
	} else {
		*result = nullptr;
	}
	return TJS_S_OK;
}

NCB_REGISTER_THREE_SUBCLASS(PointLightHelper)
Factory(PointLightHelperFactory);

NCB_THREE_METHOD(update);
};

// ======================================================================
// SpotLightHelper
// ======================================================================

static tjs_error
SpotLightHelperFactory(ThreeWrapper<SpotLightHelper> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	SpotLight* light = nullptr;
	std::optional<Color> color = std::nullopt;

	if (numparams > 0 && params[0]->Type() == tvtObject) {
		auto *lightWrapper = ncbInstanceAdaptor<ThreeWrapper<SpotLight>>::GetNativeInstance(params[0]->AsObjectNoAddRef());
		if (lightWrapper) light = lightWrapper->getRawPtr();
	}
	if (numparams > 1) color = Color((unsigned int)params[1]->AsInteger());

	if (light) {
		auto obj = SpotLightHelper::create(*light, color);
		*result = new ThreeWrapper<SpotLightHelper>(obj);
	} else {
		*result = nullptr;
	}
	return TJS_S_OK;
}

NCB_REGISTER_THREE_SUBCLASS(SpotLightHelper)
Factory(SpotLightHelperFactory);

NCB_THREE_METHOD(update);
};

// ======================================================================
// PolarGridHelper
// ======================================================================

static tjs_error
PolarGridHelperFactory(ThreeWrapper<PolarGridHelper> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	float radius = 10.0f;
	unsigned int sectors = 16;
	unsigned int rings = 8;
	unsigned int divisions = 64;
	Color color1(0x444444);
	Color color2(0x888888);

	if (numparams > 0) radius = (float)params[0]->AsReal();
	if (numparams > 1) sectors = (unsigned int)params[1]->AsInteger();
	if (numparams > 2) rings = (unsigned int)params[2]->AsInteger();
	if (numparams > 3) divisions = (unsigned int)params[3]->AsInteger();
	if (numparams > 4) color1 = Color((unsigned int)params[4]->AsInteger());
	if (numparams > 5) color2 = Color((unsigned int)params[5]->AsInteger());

	auto obj = PolarGridHelper::create(radius, sectors, rings, divisions, color1, color2);
	*result = new ThreeWrapper<PolarGridHelper>(obj);
	return TJS_S_OK;
}

NCB_REGISTER_THREE_SUBCLASS(PolarGridHelper)
Factory(PolarGridHelperFactory);

};

// ======================================================================
// PlaneHelper
// ======================================================================

static tjs_error
PlaneHelperFactory(ThreeWrapper<PlaneHelper> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	Plane plane;
	float size = 1.0f;
	Color color(0xffff00);

	if (numparams > 0 && params[0]->Type() == tvtObject) {
		auto *planeObj = ncbInstanceAdaptor<Plane>::GetNativeInstance(params[0]->AsObjectNoAddRef());
		if (planeObj) plane = *planeObj;
	}
	if (numparams > 1) size = (float)params[1]->AsReal();
	if (numparams > 2) color = Color((unsigned int)params[2]->AsInteger());

	auto obj = PlaneHelper::create(plane, size, color);
	*result = new ThreeWrapper<PlaneHelper>(obj);
	return TJS_S_OK;
}

NCB_REGISTER_THREE_SUBCLASS(PlaneHelper)
Factory(PlaneHelperFactory);

NCB_THREE_MEMBER_PROPERTY(size, float, size);
};

// ======================================================================
// LOD (Level of Detail)
// ======================================================================

static tjs_error
LODFactory(ThreeWrapper<LOD> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	auto obj = LOD::create();
	*result = new ThreeWrapper<LOD>(obj);
	return TJS_S_OK;
}

static void LODAddLevel(ThreeWrapper<LOD> *self, ThreeWrapper<Object3D> *object, float distance)
{
	if (self && object && self->getObject() && object->getObject()) {
		self->getObject()->addLevel(object->getObject(), distance);
	}
}

static void LODUpdate(ThreeWrapper<LOD> *self, ThreeWrapper<Camera> *camera)
{
	if (self && camera && self->getObject() && camera->getRawPtr()) {
		self->getObject()->update(*camera->getRawPtr());
	}
}

NCB_REGISTER_THREE_SUBCLASS(LOD)
Factory(LODFactory);

NCB_THREE_MEMBER_PROPERTY(autoUpdate, bool, autoUpdate);
NCB_THREE_METHOD(getCurrentLevel);
NCB_METHOD_PROXY(addLevel, LODAddLevel);
NCB_METHOD_PROXY(update, LODUpdate);
};

// ======================================================================
// Bone
// ======================================================================

static tjs_error
BoneFactory(ThreeWrapper<Bone> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	auto obj = Bone::create();
	*result = new ThreeWrapper<Bone>(obj);
	return TJS_S_OK;
}

NCB_REGISTER_THREE_SUBCLASS(Bone)
Factory(BoneFactory);
NCB_THREE_PROPERTY_RO(type, type);
THREE_OBJECT3D_MEMBERS();
};

// ======================================================================
// Skeleton
// ======================================================================

static tjs_error
SkeletonFactory(ThreeWrapper<Skeleton> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	std::vector<std::shared_ptr<Bone>> bones;
	
	// 配列からボーンを取得
	if (numparams > 0 && params[0]->Type() == tvtObject) {
		ncbPropAccessor arr(*params[0]);
		tjs_int count = arr.GetArrayCount();
		for (tjs_int i = 0; i < count; i++) {
			tTJSVariant v;
			if (!arr.checkVariant(i, v)) continue;
			if (v.Type() == tvtObject) {
				auto *boneWrapper = ncbInstanceAdaptor<ThreeWrapper<Bone>>::GetNativeInstance(v.AsObjectNoAddRef());
				if (boneWrapper && boneWrapper->getObject()) {
					bones.push_back(boneWrapper->getObject());
				}
			}
		}
	}

	auto obj = Skeleton::create(bones);
	*result = new ThreeWrapper<Skeleton>(obj);
	return TJS_S_OK;
}

static void Skeleton_update(ThreeWrapper<Skeleton> *self) { if (self && self->getObject()) self->getObject()->update(); }
static void Skeleton_computeBoneTexture(ThreeWrapper<Skeleton> *self) { if (self && self->getObject()) self->getObject()->computeBoneTexture(); }

NCB_REGISTER_THREE_SUBCLASS(Skeleton)
Factory(SkeletonFactory);
// uuid プロパティは const メソッドのため省略
NCB_THREE_MEMBER_PROPERTY(frame, int, frame);
NCB_THREE_METHOD(init);
NCB_THREE_METHOD(calculateInverses);
NCB_THREE_METHOD(pose);
NCB_METHOD_PROXY(update, Skeleton_update);
NCB_METHOD_PROXY(computeBoneTexture, Skeleton_computeBoneTexture);
NCB_THREE_METHOD(dispose);
};

// ======================================================================
// SkinnedMesh
// ======================================================================

static tjs_error
SkinnedMeshFactory(ThreeWrapper<SkinnedMesh> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	std::shared_ptr<BufferGeometry> geometry = nullptr;
	std::shared_ptr<Material> material = nullptr;

	if (numparams > 0 && params[0]->Type() == tvtObject) {
		auto *geoWrapper = ncbInstanceAdaptor<ThreeWrapper<BufferGeometry>>::GetNativeInstance(params[0]->AsObjectNoAddRef());
		if (geoWrapper) geometry = geoWrapper->getObject();
	}
	if (numparams > 1 && params[1]->Type() == tvtObject) {
		auto *matWrapper = ncbInstanceAdaptor<ThreeWrapper<Material>>::GetNativeInstance(params[1]->AsObjectNoAddRef());
		if (matWrapper) material = matWrapper->getObject();
	}

	auto obj = SkinnedMesh::create(geometry, material);
	*result = new ThreeWrapper<SkinnedMesh>(obj);
	return TJS_S_OK;
}

static void SkinnedMeshBind(ThreeWrapper<SkinnedMesh> *self, ThreeWrapper<Skeleton> *skeleton)
{
	if (self && skeleton && self->getObject() && skeleton->getObject()) {
		self->getObject()->bind(skeleton->getObject());
	}
}

NCB_REGISTER_THREE_SUBCLASS(SkinnedMesh)
Factory(SkinnedMeshFactory);

NCB_METHOD_PROXY(bind, SkinnedMeshBind);
NCB_THREE_METHOD(pose);
NCB_THREE_METHOD(normalizeSkinWeights);
};

// ======================================================================
// Sky
// ======================================================================

static tjs_error
SkyFactory(ThreeWrapper<Sky> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	auto obj = Sky::create();
	*result = new ThreeWrapper<Sky>(obj);
	return TJS_S_OK;
}

NCB_REGISTER_THREE_SUBCLASS(Sky)
Factory(SkyFactory);

};

// ======================================================================
// Water
// ======================================================================

static tjs_error
WaterFactory(ThreeWrapper<Water> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	std::shared_ptr<BufferGeometry> geometry = nullptr;
	Water::Options options;

	if (numparams > 0 && params[0]->Type() == tvtObject) {
		auto *geoWrapper = ncbInstanceAdaptor<ThreeWrapper<BufferGeometry>>::GetNativeInstance(params[0]->AsObjectNoAddRef());
		if (geoWrapper) geometry = geoWrapper->getObject();
	}
	// オプションは辞書配列から設定可能にできるが、簡略化のためデフォルト
	
	if (geometry) {
		auto obj = Water::create(geometry, options);
		*result = new ThreeWrapper<Water>(obj);
	} else {
		*result = nullptr;
	}
	return TJS_S_OK;
}

NCB_REGISTER_THREE_SUBCLASS(Water)
Factory(WaterFactory);

};

// ======================================================================
// Reflector
// ======================================================================

static tjs_error
ReflectorFactory(ThreeWrapper<Reflector> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	std::shared_ptr<BufferGeometry> geometry = nullptr;
	Reflector::Options options;

	if (numparams > 0 && params[0]->Type() == tvtObject) {
		auto *geoWrapper = ncbInstanceAdaptor<ThreeWrapper<BufferGeometry>>::GetNativeInstance(params[0]->AsObjectNoAddRef());
		if (geoWrapper) geometry = geoWrapper->getObject();
	}
	// オプションは辞書配列から設定可能にできるが、簡略化のためデフォルト
	
	if (geometry) {
		auto obj = Reflector::create(geometry, options);
		*result = new ThreeWrapper<Reflector>(obj);
	} else {
		*result = nullptr;
	}
	return TJS_S_OK;
}

NCB_REGISTER_THREE_SUBCLASS(Reflector)
Factory(ReflectorFactory);

};

// ======================================================================
// LineCurve (2D)
// ======================================================================

static tjs_error
LineCurveFactory(ThreeWrapper<LineCurve> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	Vector2 v1, v2;

	if (numparams > 0 && params[0]->Type() == tvtObject) {
		ncbPropAccessor pt(*params[0]);
		if (IsArray(*params[0])) {
			v1 = Vector2((float)pt.getRealValue(0), (float)pt.getRealValue(1));
		} else {
			v1 = Vector2((float)pt.getRealValue(TJS_W("x")), (float)pt.getRealValue(TJS_W("y")));
		}
	}
	if (numparams > 1 && params[1]->Type() == tvtObject) {
		ncbPropAccessor pt(*params[1]);
		if (IsArray(*params[1])) {
			v2 = Vector2((float)pt.getRealValue(0), (float)pt.getRealValue(1));
		} else {
			v2 = Vector2((float)pt.getRealValue(TJS_W("x")), (float)pt.getRealValue(TJS_W("y")));
		}
	}

	auto obj = std::make_shared<LineCurve>(v1, v2);
	*result = new ThreeWrapper<LineCurve>(obj);
	return TJS_S_OK;
}

static Vector2 LineCurveGetPoint(ThreeWrapper<LineCurve> *self, float t)
{
	Vector2 result;
	if (self && self->getObject()) {
		self->getObject()->getPoint(t, result);
	}
	return result;
}

static Vector2 LineCurveGetPointAt(ThreeWrapper<LineCurve> *self, float u)
{
	Vector2 result;
	if (self && self->getObject()) {
		self->getObject()->getPointAt(u, result);
	}
	return result;
}

static Vector2 LineCurveGetTangent(ThreeWrapper<LineCurve> *self, float t)
{
	Vector2 result;
	if (self && self->getObject()) {
		self->getObject()->getTangent(t, result);
	}
	return result;
}

NCB_REGISTER_THREE_SUBCLASS(LineCurve)
Factory(LineCurveFactory);
NCB_THREE_METHOD(getLength);
NCB_METHOD_PROXY(getPoint, LineCurveGetPoint);
NCB_METHOD_PROXY(getPointAt, LineCurveGetPointAt);
NCB_METHOD_PROXY(getTangent, LineCurveGetTangent);
};

// ======================================================================
// LineCurve3 (3D)
// ======================================================================

static tjs_error
LineCurve3Factory(ThreeWrapper<LineCurve3> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	Vector3 v1, v2;

	if (numparams > 0 && params[0]->Type() == tvtObject) {
		ncbPropAccessor pt(*params[0]);
		if (IsArray(*params[0])) {
			v1 = Vector3((float)pt.getRealValue(0), (float)pt.getRealValue(1), (float)pt.getRealValue(2));
		} else {
			v1 = Vector3((float)pt.getRealValue(TJS_W("x")), (float)pt.getRealValue(TJS_W("y")), (float)pt.getRealValue(TJS_W("z")));
		}
	}
	if (numparams > 1 && params[1]->Type() == tvtObject) {
		ncbPropAccessor pt(*params[1]);
		if (IsArray(*params[1])) {
			v2 = Vector3((float)pt.getRealValue(0), (float)pt.getRealValue(1), (float)pt.getRealValue(2));
		} else {
			v2 = Vector3((float)pt.getRealValue(TJS_W("x")), (float)pt.getRealValue(TJS_W("y")), (float)pt.getRealValue(TJS_W("z")));
		}
	}

	auto obj = std::make_shared<LineCurve3>(v1, v2);
	*result = new ThreeWrapper<LineCurve3>(obj);
	return TJS_S_OK;
}

static Vector3 LineCurve3GetPoint(ThreeWrapper<LineCurve3> *self, float t)
{
	Vector3 result;
	if (self && self->getObject()) {
		self->getObject()->getPoint(t, result);
	}
	return result;
}

static Vector3 LineCurve3GetPointAt(ThreeWrapper<LineCurve3> *self, float u)
{
	Vector3 result;
	if (self && self->getObject()) {
		self->getObject()->getPointAt(u, result);
	}
	return result;
}

static Vector3 LineCurve3GetTangent(ThreeWrapper<LineCurve3> *self, float t)
{
	Vector3 result;
	if (self && self->getObject()) {
		self->getObject()->getTangent(t, result);
	}
	return result;
}

NCB_REGISTER_THREE_SUBCLASS(LineCurve3)
Factory(LineCurve3Factory);
NCB_THREE_METHOD(getLength);
NCB_METHOD_PROXY(getPoint, LineCurve3GetPoint);
NCB_METHOD_PROXY(getPointAt, LineCurve3GetPointAt);
NCB_METHOD_PROXY(getTangent, LineCurve3GetTangent);
};

// ======================================================================
// QuadraticBezierCurve (2D)
// ======================================================================

static tjs_error
QuadraticBezierCurveFactory(ThreeWrapper<QuadraticBezierCurve> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	Vector2 v0, v1, v2;

	auto readVec2 = [](tTJSVariant* param) -> Vector2 {
		if (param->Type() == tvtObject) {
			ncbPropAccessor pt(*param);
			if (IsArray(*param)) {
				return Vector2((float)pt.getRealValue(0), (float)pt.getRealValue(1));
			} else {
				return Vector2((float)pt.getRealValue(TJS_W("x")), (float)pt.getRealValue(TJS_W("y")));
			}
		}
		return Vector2();
	};

	if (numparams > 0) v0 = readVec2(params[0]);
	if (numparams > 1) v1 = readVec2(params[1]);
	if (numparams > 2) v2 = readVec2(params[2]);

	auto obj = std::make_shared<QuadraticBezierCurve>(v0, v1, v2);
	*result = new ThreeWrapper<QuadraticBezierCurve>(obj);
	return TJS_S_OK;
}

static Vector2 QuadraticBezierCurveGetPoint(ThreeWrapper<QuadraticBezierCurve> *self, float t)
{
	Vector2 result;
	if (self && self->getObject()) {
		self->getObject()->getPoint(t, result);
	}
	return result;
}

NCB_REGISTER_THREE_SUBCLASS(QuadraticBezierCurve)
Factory(QuadraticBezierCurveFactory);
NCB_THREE_METHOD(getLength);
NCB_METHOD_PROXY(getPoint, QuadraticBezierCurveGetPoint);
};

// ======================================================================
// QuadraticBezierCurve3 (3D)
// ======================================================================

static tjs_error
QuadraticBezierCurve3Factory(ThreeWrapper<QuadraticBezierCurve3> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	Vector3 v0, v1, v2;

	auto readVec3 = [](tTJSVariant* param) -> Vector3 {
		if (param->Type() == tvtObject) {
			ncbPropAccessor pt(*param);
			if (IsArray(*param)) {
				return Vector3((float)pt.getRealValue(0), (float)pt.getRealValue(1), (float)pt.getRealValue(2));
			} else {
				return Vector3((float)pt.getRealValue(TJS_W("x")), (float)pt.getRealValue(TJS_W("y")), (float)pt.getRealValue(TJS_W("z")));
			}
		}
		return Vector3();
	};

	if (numparams > 0) v0 = readVec3(params[0]);
	if (numparams > 1) v1 = readVec3(params[1]);
	if (numparams > 2) v2 = readVec3(params[2]);

	auto obj = std::make_shared<QuadraticBezierCurve3>(v0, v1, v2);
	*result = new ThreeWrapper<QuadraticBezierCurve3>(obj);
	return TJS_S_OK;
}

static Vector3 QuadraticBezierCurve3GetPoint(ThreeWrapper<QuadraticBezierCurve3> *self, float t)
{
	Vector3 result;
	if (self && self->getObject()) {
		self->getObject()->getPoint(t, result);
	}
	return result;
}

NCB_REGISTER_THREE_SUBCLASS(QuadraticBezierCurve3)
Factory(QuadraticBezierCurve3Factory);
NCB_THREE_METHOD(getLength);
NCB_METHOD_PROXY(getPoint, QuadraticBezierCurve3GetPoint);
};

// ======================================================================
// CubicBezierCurve (2D)
// ======================================================================

static tjs_error
CubicBezierCurveFactory(ThreeWrapper<CubicBezierCurve> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	Vector2 v0, v1, v2, v3;

	auto readVec2 = [](tTJSVariant* param) -> Vector2 {
		if (param->Type() == tvtObject) {
			ncbPropAccessor pt(*param);
			if (IsArray(*param)) {
				return Vector2((float)pt.getRealValue(0), (float)pt.getRealValue(1));
			} else {
				return Vector2((float)pt.getRealValue(TJS_W("x")), (float)pt.getRealValue(TJS_W("y")));
			}
		}
		return Vector2();
	};

	if (numparams > 0) v0 = readVec2(params[0]);
	if (numparams > 1) v1 = readVec2(params[1]);
	if (numparams > 2) v2 = readVec2(params[2]);
	if (numparams > 3) v3 = readVec2(params[3]);

	auto obj = std::make_shared<CubicBezierCurve>(v0, v1, v2, v3);
	*result = new ThreeWrapper<CubicBezierCurve>(obj);
	return TJS_S_OK;
}

static Vector2 CubicBezierCurveGetPoint(ThreeWrapper<CubicBezierCurve> *self, float t)
{
	Vector2 result;
	if (self && self->getObject()) {
		self->getObject()->getPoint(t, result);
	}
	return result;
}

NCB_REGISTER_THREE_SUBCLASS(CubicBezierCurve)
Factory(CubicBezierCurveFactory);
NCB_THREE_METHOD(getLength);
NCB_METHOD_PROXY(getPoint, CubicBezierCurveGetPoint);
};

// ======================================================================
// CubicBezierCurve3 (3D)
// ======================================================================

static tjs_error
CubicBezierCurve3Factory(ThreeWrapper<CubicBezierCurve3> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	Vector3 v0, v1, v2, v3;

	auto readVec3 = [](tTJSVariant* param) -> Vector3 {
		if (param->Type() == tvtObject) {
			ncbPropAccessor pt(*param);
			if (IsArray(*param)) {
				return Vector3((float)pt.getRealValue(0), (float)pt.getRealValue(1), (float)pt.getRealValue(2));
			} else {
				return Vector3((float)pt.getRealValue(TJS_W("x")), (float)pt.getRealValue(TJS_W("y")), (float)pt.getRealValue(TJS_W("z")));
			}
		}
		return Vector3();
	};

	if (numparams > 0) v0 = readVec3(params[0]);
	if (numparams > 1) v1 = readVec3(params[1]);
	if (numparams > 2) v2 = readVec3(params[2]);
	if (numparams > 3) v3 = readVec3(params[3]);

	auto obj = std::make_shared<CubicBezierCurve3>(v0, v1, v2, v3);
	*result = new ThreeWrapper<CubicBezierCurve3>(obj);
	return TJS_S_OK;
}

static Vector3 CubicBezierCurve3GetPoint(ThreeWrapper<CubicBezierCurve3> *self, float t)
{
	Vector3 result;
	if (self && self->getObject()) {
		self->getObject()->getPoint(t, result);
	}
	return result;
}

NCB_REGISTER_THREE_SUBCLASS(CubicBezierCurve3)
Factory(CubicBezierCurve3Factory);
NCB_THREE_METHOD(getLength);
NCB_METHOD_PROXY(getPoint, CubicBezierCurve3GetPoint);
};

// ======================================================================
// SplineCurve (2D)
// ======================================================================

static tjs_error
SplineCurveFactory(ThreeWrapper<SplineCurve> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	std::vector<Vector2> points;

	if (numparams > 0 && params[0]->Type() == tvtObject) {
		ncbPropAccessor arr(*params[0]);
		tjs_int count = arr.GetArrayCount();
		for (tjs_int i = 0; i < count; i++) {
			tTJSVariant v;
			if (!arr.checkVariant(i, v)) continue;
			if (v.Type() == tvtObject) {
				ncbPropAccessor pt(v);
				if (IsArray(v)) {
					points.push_back(Vector2((float)pt.getRealValue(0), (float)pt.getRealValue(1)));
				} else {
					points.push_back(Vector2((float)pt.getRealValue(TJS_W("x")), (float)pt.getRealValue(TJS_W("y"))));
				}
			}
		}
	}

	auto obj = std::make_shared<SplineCurve>(points);
	*result = new ThreeWrapper<SplineCurve>(obj);
	return TJS_S_OK;
}

static Vector2 SplineCurveGetPoint(ThreeWrapper<SplineCurve> *self, float t)
{
	Vector2 result;
	if (self && self->getObject()) {
		self->getObject()->getPoint(t, result);
	}
	return result;
}

NCB_REGISTER_THREE_SUBCLASS(SplineCurve)
Factory(SplineCurveFactory);
NCB_THREE_METHOD(getLength);
NCB_METHOD_PROXY(getPoint, SplineCurveGetPoint);
};

// ======================================================================
// EllipseCurve (2D)
// ======================================================================

static tjs_error
EllipseCurveFactory(ThreeWrapper<EllipseCurve> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	float aX = 0, aY = 0;
	float xRadius = 1, yRadius = 1;
	float aStartAngle = 0, aEndAngle = math::TWO_PI;
	bool aClockwise = false;
	float aRotation = 0;

	if (numparams > 0) aX = (float)params[0]->AsReal();
	if (numparams > 1) aY = (float)params[1]->AsReal();
	if (numparams > 2) xRadius = (float)params[2]->AsReal();
	if (numparams > 3) yRadius = (float)params[3]->AsReal();
	if (numparams > 4) aStartAngle = (float)params[4]->AsReal();
	if (numparams > 5) aEndAngle = (float)params[5]->AsReal();
	if (numparams > 6) aClockwise = (bool)params[6]->AsInteger();
	if (numparams > 7) aRotation = (float)params[7]->AsReal();

	auto obj = std::make_shared<EllipseCurve>(aX, aY, xRadius, yRadius, aStartAngle, aEndAngle, aClockwise, aRotation);
	*result = new ThreeWrapper<EllipseCurve>(obj);
	return TJS_S_OK;
}

static Vector2 EllipseCurveGetPoint(ThreeWrapper<EllipseCurve> *self, float t)
{
	Vector2 result;
	if (self && self->getObject()) {
		self->getObject()->getPoint(t, result);
	}
	return result;
}

NCB_REGISTER_THREE_SUBCLASS(EllipseCurve)
Factory(EllipseCurveFactory);
NCB_THREE_MEMBER_PROPERTY(aX, float, aX);
NCB_THREE_MEMBER_PROPERTY(aY, float, aY);
NCB_THREE_MEMBER_PROPERTY(xRadius, float, xRadius);
NCB_THREE_MEMBER_PROPERTY(yRadius, float, yRadius);
NCB_THREE_MEMBER_PROPERTY(aStartAngle, float, aStartAngle);
NCB_THREE_MEMBER_PROPERTY(aEndAngle, float, aEndAngle);
NCB_THREE_MEMBER_PROPERTY(aClockwise, bool, aClockwise);
NCB_THREE_MEMBER_PROPERTY(aRotation, float, aRotation);
NCB_THREE_METHOD(getLength);
NCB_METHOD_PROXY(getPoint, EllipseCurveGetPoint);
};

// ======================================================================
// Path (CurvePath<Vector2>)
// ======================================================================

static tjs_error
PathFactory(ThreeWrapper<Path> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	std::vector<Vector2> points;

	if (numparams > 0 && params[0]->Type() == tvtObject) {
		ncbPropAccessor arr(*params[0]);
		tjs_int count = arr.GetArrayCount();
		for (tjs_int i = 0; i < count; i++) {
			tTJSVariant v;
			if (!arr.checkVariant(i, v)) continue;
			if (v.Type() == tvtObject) {
				ncbPropAccessor pt(v);
				if (IsArray(v)) {
					points.push_back(Vector2((float)pt.getRealValue(0), (float)pt.getRealValue(1)));
				} else {
					points.push_back(Vector2((float)pt.getRealValue(TJS_W("x")), (float)pt.getRealValue(TJS_W("y"))));
				}
			}
		}
	}

	auto obj = std::make_shared<Path>(points);
	*result = new ThreeWrapper<Path>(obj);
	return TJS_S_OK;
}

static Vector2 PathGetPoint(ThreeWrapper<Path> *self, float t)
{
	Vector2 result;
	if (self && self->getObject()) {
		self->getObject()->getPoint(t, result);
	}
	return result;
}

static Vector2 PathGetPointAt(ThreeWrapper<Path> *self, float u)
{
	Vector2 result;
	if (self && self->getObject()) {
		self->getObject()->getPointAt(u, result);
	}
	return result;
}

// Path描画メソッド用プロキシ
static void PathMoveTo(ThreeWrapper<Path> *self, float x, float y)
{
	if (self && self->getObject()) {
		self->getObject()->moveTo(x, y);
	}
}

static void PathLineTo(ThreeWrapper<Path> *self, float x, float y)
{
	if (self && self->getObject()) {
		self->getObject()->lineTo(x, y);
	}
}

static void PathQuadraticCurveTo(ThreeWrapper<Path> *self, float cpX, float cpY, float x, float y)
{
	if (self && self->getObject()) {
		self->getObject()->quadraticCurveTo(cpX, cpY, x, y);
	}
}

static void PathBezierCurveTo(ThreeWrapper<Path> *self, float cp1x, float cp1y, float cp2x, float cp2y, float x, float y)
{
	if (self && self->getObject()) {
		self->getObject()->bezierCurveTo(cp1x, cp1y, cp2x, cp2y, x, y);
	}
}

static void PathArc(ThreeWrapper<Path> *self, float x, float y, float radius, float startAngle, float endAngle, bool clockwise)
{
	if (self && self->getObject()) {
		self->getObject()->arc(x, y, radius, startAngle, endAngle, clockwise);
	}
}

static void PathAbsarc(ThreeWrapper<Path> *self, float x, float y, float radius, float startAngle, float endAngle, bool clockwise)
{
	if (self && self->getObject()) {
		self->getObject()->absarc(x, y, radius, startAngle, endAngle, clockwise);
	}
}

static void PathAbsellipse(ThreeWrapper<Path> *self, float x, float y, float xRadius, float yRadius, float startAngle, float endAngle, bool clockwise, float rotation)
{
	if (self && self->getObject()) {
		self->getObject()->absellipse(x, y, xRadius, yRadius, startAngle, endAngle, clockwise, rotation);
	}
}

static tjs_error PathSplineThru(tTJSVariant *result, tjs_int numparams, tTJSVariant **params, ThreeWrapper<Path> *self)
{
	if (numparams < 1) return TJS_E_BADPARAMCOUNT;
	if (self && self->getObject() && params[0]->Type() == tvtObject) {
		std::vector<Vector2> pts;
		ncbPropAccessor arr(*params[0]);
		tjs_int count = arr.GetArrayCount();
		for (tjs_int i = 0; i < count; i++) {
			tTJSVariant v;
			if (!arr.checkVariant(i, v)) continue;
			if (v.Type() == tvtObject) {
				ncbPropAccessor pt(v);
				if (IsArray(v)) {
					pts.push_back(Vector2((float)pt.getRealValue(0), (float)pt.getRealValue(1)));
				} else {
					pts.push_back(Vector2((float)pt.getRealValue(TJS_W("x")), (float)pt.getRealValue(TJS_W("y"))));
				}
			}
		}
		self->getObject()->splineThru(pts);
	}
	return TJS_S_OK;
}

static void PathClosePath(ThreeWrapper<Path> *self)
{
	if (self && self->getObject()) {
		self->getObject()->closePath();
	}
}

NCB_REGISTER_THREE_SUBCLASS(Path)
Factory(PathFactory);
NCB_THREE_MEMBER_PROPERTY(autoClose, bool, autoClose);
NCB_THREE_METHOD(getLength);
NCB_METHOD_PROXY(getPoint, PathGetPoint);
NCB_METHOD_PROXY(getPointAt, PathGetPointAt);
NCB_METHOD_PROXY(moveTo, PathMoveTo);
NCB_METHOD_PROXY(lineTo, PathLineTo);
NCB_METHOD_PROXY(quadraticCurveTo, PathQuadraticCurveTo);
NCB_METHOD_PROXY(bezierCurveTo, PathBezierCurveTo);
NCB_METHOD_PROXY(arc, PathArc);
NCB_METHOD_PROXY(absarc, PathAbsarc);
NCB_METHOD_PROXY(absellipse, PathAbsellipse);
RawCallback(TJS_W("splineThru"), &PathSplineThru, TJS_STATICMEMBER);
NCB_METHOD_PROXY(closePath, PathClosePath);
};

// ======================================================================
// Shape (extends Path)
// ======================================================================

static tjs_error
ShapeFactory(ThreeWrapper<Shape> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	std::vector<Vector2> points;

	if (numparams > 0 && params[0]->Type() == tvtObject) {
		ncbPropAccessor arr(*params[0]);
		tjs_int count = arr.GetArrayCount();
		for (tjs_int i = 0; i < count; i++) {
			tTJSVariant v;
			if (!arr.checkVariant(i, v)) continue;
			if (v.Type() == tvtObject) {
				ncbPropAccessor pt(v);
				if (IsArray(v)) {
					points.push_back(Vector2((float)pt.getRealValue(0), (float)pt.getRealValue(1)));
				} else {
					points.push_back(Vector2((float)pt.getRealValue(TJS_W("x")), (float)pt.getRealValue(TJS_W("y"))));
				}
			}
		}
	}

	auto obj = std::make_shared<Shape>(points);
	*result = new ThreeWrapper<Shape>(obj);
	return TJS_S_OK;
}

static Vector2 ShapeGetPoint(ThreeWrapper<Shape> *self, float t)
{
	Vector2 result;
	if (self && self->getObject()) {
		self->getObject()->getPoint(t, result);
	}
	return result;
}

static Vector2 ShapeGetPointAt(ThreeWrapper<Shape> *self, float u)
{
	Vector2 result;
	if (self && self->getObject()) {
		self->getObject()->getPointAt(u, result);
	}
	return result;
}

static void ShapeMoveTo(ThreeWrapper<Shape> *self, float x, float y)
{
	if (self && self->getObject()) {
		self->getObject()->moveTo(x, y);
	}
}

static void ShapeLineTo(ThreeWrapper<Shape> *self, float x, float y)
{
	if (self && self->getObject()) {
		self->getObject()->lineTo(x, y);
	}
}

static void ShapeQuadraticCurveTo(ThreeWrapper<Shape> *self, float cpX, float cpY, float x, float y)
{
	if (self && self->getObject()) {
		self->getObject()->quadraticCurveTo(cpX, cpY, x, y);
	}
}

static void ShapeBezierCurveTo(ThreeWrapper<Shape> *self, float cp1x, float cp1y, float cp2x, float cp2y, float x, float y)
{
	if (self && self->getObject()) {
		self->getObject()->bezierCurveTo(cp1x, cp1y, cp2x, cp2y, x, y);
	}
}

static void ShapeArc(ThreeWrapper<Shape> *self, float x, float y, float radius, float startAngle, float endAngle, bool clockwise)
{
	if (self && self->getObject()) {
		self->getObject()->arc(x, y, radius, startAngle, endAngle, clockwise);
	}
}

static void ShapeAbsarc(ThreeWrapper<Shape> *self, float x, float y, float radius, float startAngle, float endAngle, bool clockwise)
{
	if (self && self->getObject()) {
		self->getObject()->absarc(x, y, radius, startAngle, endAngle, clockwise);
	}
}

static void ShapeAbsellipse(ThreeWrapper<Shape> *self, float x, float y, float xRadius, float yRadius, float startAngle, float endAngle, bool clockwise, float rotation)
{
	if (self && self->getObject()) {
		self->getObject()->absellipse(x, y, xRadius, yRadius, startAngle, endAngle, clockwise, rotation);
	}
}

static tjs_error ShapeSplineThru(tTJSVariant *result, tjs_int numparams, tTJSVariant **params, ThreeWrapper<Shape> *self)
{
	if (numparams < 1) return TJS_E_BADPARAMCOUNT;
	if (self && self->getObject() && params[0]->Type() == tvtObject) {
		std::vector<Vector2> pts;
		ncbPropAccessor arr(*params[0]);
		tjs_int count = arr.GetArrayCount();
		for (tjs_int i = 0; i < count; i++) {
			tTJSVariant v;
			if (!arr.checkVariant(i, v)) continue;
			if (v.Type() == tvtObject) {
				ncbPropAccessor pt(v);
				if (IsArray(v)) {
					pts.push_back(Vector2((float)pt.getRealValue(0), (float)pt.getRealValue(1)));
				} else {
					pts.push_back(Vector2((float)pt.getRealValue(TJS_W("x")), (float)pt.getRealValue(TJS_W("y"))));
				}
			}
		}
		self->getObject()->splineThru(pts);
	}
	return TJS_S_OK;
}

static void ShapeClosePath(ThreeWrapper<Shape> *self)
{
	if (self && self->getObject()) {
		self->getObject()->closePath();
	}
}

// Shapeに穴を追加
static void ShapeAddHole(ThreeWrapper<Shape> *self, ThreeWrapper<Path> *hole)
{
	if (self && self->getObject() && hole && hole->getObject()) {
		self->getObject()->holes.push_back(hole->getObject());
	}
}

// 穴のPathを取得
static ThreeWrapper<Path>* ShapeGetHole(ThreeWrapper<Shape> *self, int index)
{
	if (self && self->getObject() && index >= 0 && index < (int)self->getObject()->holes.size()) {
		return new ThreeWrapper<Path>(self->getObject()->holes[index]);
	}
	return nullptr;
}

// 穴の数を取得
static int ShapeGetHoleCount(ThreeWrapper<Shape> *self)
{
	if (self && self->getObject()) {
		return (int)self->getObject()->holes.size();
	}
	return 0;
}

// uuid取得
static ttstr ShapeGetUUID(ThreeWrapper<Shape> *self)
{
	if (self && self->getObject()) {
		return ttstr(self->getObject()->uuid().c_str());
	}
	return TJS_W("");
}

NCB_REGISTER_THREE_SUBCLASS(Shape)
Factory(ShapeFactory);
NCB_THREE_MEMBER_PROPERTY(autoClose, bool, autoClose);
NCB_THREE_METHOD(getLength);
NCB_METHOD_PROXY(getPoint, ShapeGetPoint);
NCB_METHOD_PROXY(getPointAt, ShapeGetPointAt);
NCB_METHOD_PROXY(moveTo, ShapeMoveTo);
NCB_METHOD_PROXY(lineTo, ShapeLineTo);
NCB_METHOD_PROXY(quadraticCurveTo, ShapeQuadraticCurveTo);
NCB_METHOD_PROXY(bezierCurveTo, ShapeBezierCurveTo);
NCB_METHOD_PROXY(arc, ShapeArc);
NCB_METHOD_PROXY(absarc, ShapeAbsarc);
NCB_METHOD_PROXY(absellipse, ShapeAbsellipse);
RawCallback(TJS_W("splineThru"), &ShapeSplineThru, TJS_STATICMEMBER);
NCB_METHOD_PROXY(closePath, ShapeClosePath);
NCB_METHOD_PROXY(addHole, ShapeAddHole);
NCB_METHOD_PROXY(getHole, ShapeGetHole);
NCB_METHOD_PROXY(getHoleCount, ShapeGetHoleCount);
NCB_METHOD_PROXY(uuid, ShapeGetUUID);
};

// ======================================================================
// ShapeGeometry
// ======================================================================

static tjs_error
ShapeGeometryFactory(ThreeWrapper<ShapeGeometry> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	unsigned int curveSegments = 12;

	if (numparams < 1 || params[0]->Type() != tvtObject) {
		return TJS_E_INVALIDPARAM;
	}

	if (numparams > 1) curveSegments = (unsigned int)params[1]->AsInteger();

	// 単一のShapeかShapeの配列かを判定
	ncbPropAccessor arr(*params[0]);
	
	// Shapeラッパーかどうかチェック
	ThreeWrapper<Shape> *shapeWrapper = ncbInstanceAdaptor<ThreeWrapper<Shape>>::GetNativeInstance(params[0]->AsObjectNoAddRef());
	
	if (shapeWrapper && shapeWrapper->getObject()) {
		// 単一のShape
		auto obj = ShapeGeometry::create(*shapeWrapper->getObject(), curveSegments);
		*result = new ThreeWrapper<ShapeGeometry>(obj);
	} else {
		// Shapeの配列
		std::vector<Shape> shapes;
		tjs_int count = arr.GetArrayCount();
		for (tjs_int i = 0; i < count; i++) {
			tTJSVariant v;
			if (!arr.checkVariant(i, v)) continue;
			if (v.Type() == tvtObject) {
				ThreeWrapper<Shape> *sw = ncbInstanceAdaptor<ThreeWrapper<Shape>>::GetNativeInstance(v.AsObjectNoAddRef());
				if (sw && sw->getObject()) {
					shapes.push_back(*sw->getObject());
				}
			}
		}
		if (shapes.empty()) {
			return TJS_E_INVALIDPARAM;
		}
		auto obj = ShapeGeometry::create(shapes, curveSegments);
		*result = new ThreeWrapper<ShapeGeometry>(obj);
	}

	return TJS_S_OK;
}

NCB_REGISTER_THREE_SUBCLASS(ShapeGeometry)
Factory(ShapeGeometryFactory);
};

// ======================================================================
// ExtrudeGeometry
// ======================================================================

static tjs_error
ExtrudeGeometryFactory(ThreeWrapper<ExtrudeGeometry> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	if (numparams < 1 || params[0]->Type() != tvtObject) {
		return TJS_E_INVALIDPARAM;
	}

	ExtrudeGeometry::Options options;

	// オプションを解析
	if (numparams > 1 && params[1]->Type() == tvtObject) {
		ncbPropAccessor opt(*params[1]);
		if (opt.HasValue(TJS_W("curveSegments"))) options.curveSegments = (unsigned int)opt.getIntValue(TJS_W("curveSegments"));
		if (opt.HasValue(TJS_W("steps"))) options.steps = (unsigned int)opt.getIntValue(TJS_W("steps"));
		if (opt.HasValue(TJS_W("depth"))) options.depth = (float)opt.getRealValue(TJS_W("depth"));
		if (opt.HasValue(TJS_W("bevelEnabled"))) options.bevelEnabled = (bool)opt.getIntValue(TJS_W("bevelEnabled"));
		if (opt.HasValue(TJS_W("bevelThickness"))) options.bevelThickness = (float)opt.getRealValue(TJS_W("bevelThickness"));
		if (opt.HasValue(TJS_W("bevelSize"))) options.bevelSize = (float)opt.getRealValue(TJS_W("bevelSize"));
		if (opt.HasValue(TJS_W("bevelOffset"))) options.bevelOffset = (float)opt.getRealValue(TJS_W("bevelOffset"));
		if (opt.HasValue(TJS_W("bevelSegments"))) options.bevelSegments = (unsigned int)opt.getIntValue(TJS_W("bevelSegments"));
		// extrudePathは後で対応が必要（Curve3への参照）
	}

	// 単一のShapeかShapeの配列かを判定
	ncbPropAccessor arr(*params[0]);
	
	// Shapeラッパーかどうかチェック
	ThreeWrapper<Shape> *shapeWrapper = ncbInstanceAdaptor<ThreeWrapper<Shape>>::GetNativeInstance(params[0]->AsObjectNoAddRef());
	
	std::vector<Shape> shapes;
	if (shapeWrapper && shapeWrapper->getObject()) {
		// 単一のShape
		shapes.push_back(*shapeWrapper->getObject());
	} else {
		// Shapeの配列
		tjs_int count = arr.GetArrayCount();
		for (tjs_int i = 0; i < count; i++) {
			tTJSVariant v;
			if (!arr.checkVariant(i, v)) continue;
			if (v.Type() == tvtObject) {
				ThreeWrapper<Shape> *sw = ncbInstanceAdaptor<ThreeWrapper<Shape>>::GetNativeInstance(v.AsObjectNoAddRef());
				if (sw && sw->getObject()) {
					shapes.push_back(*sw->getObject());
				}
			}
		}
	}

	if (shapes.empty()) {
		return TJS_E_INVALIDPARAM;
	}

	auto obj = ExtrudeGeometry::create(shapes, options);
	*result = new ThreeWrapper<ExtrudeGeometry>(obj);
	return TJS_S_OK;
}

NCB_REGISTER_THREE_SUBCLASS(ExtrudeGeometry)
Factory(ExtrudeGeometryFactory);
};

// ======================================================================
// CatmullRomCurve3
// ======================================================================

static tjs_error
CatmullRomCurve3Factory(ThreeWrapper<CatmullRomCurve3> **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	std::vector<Vector3> points;
	bool closed = false;
	CatmullRomCurve3::CurveType curveType = CatmullRomCurve3::centripetal;
	float tension = 0.5f;

	// 点の配列を取得
	if (numparams > 0 && params[0]->Type() == tvtObject) {
		ncbPropAccessor arr(*params[0]);
		tjs_int count = arr.GetArrayCount();
		for (tjs_int i = 0; i < count; i++) {
			tTJSVariant v;
			if (!arr.checkVariant(i, v)) continue;
			if (v.Type() == tvtObject) {
				ncbPropAccessor pt(v);
				if (IsArray(v)) {
					points.push_back(Vector3((float)pt.getRealValue(0), (float)pt.getRealValue(1), (float)pt.getRealValue(2)));
				} else {
					points.push_back(Vector3((float)pt.getRealValue(TJS_W("x")), (float)pt.getRealValue(TJS_W("y")), (float)pt.getRealValue(TJS_W("z"))));
				}
			}
		}
	}
	if (numparams > 1) closed = (bool)params[1]->AsInteger();
	if (numparams > 2) curveType = (CatmullRomCurve3::CurveType)params[2]->AsInteger();
	if (numparams > 3) tension = (float)params[3]->AsReal();

	auto obj = std::make_shared<CatmullRomCurve3>(points, closed, curveType, tension);
	*result = new ThreeWrapper<CatmullRomCurve3>(obj);
	return TJS_S_OK;
}

static Vector3 CatmullRomCurve3GetPoint(ThreeWrapper<CatmullRomCurve3> *self, float t)
{
	Vector3 result;
	if (self && self->getObject()) {
		self->getObject()->getPoint(t, result);
	}
	return result;
}

NCB_REGISTER_THREE_SUBCLASS(CatmullRomCurve3)
Factory(CatmullRomCurve3Factory);
NCB_THREE_MEMBER_PROPERTY(closed, bool, closed);
NCB_THREE_MEMBER_PROPERTY(tension, float, tension);
NCB_THREE_METHOD(getLength);
NCB_METHOD_PROXY(getPoint, CatmullRomCurve3GetPoint);
};

// ======================================================================
// クラス登録 (Threepp グローバルオブジェクト)
// ======================================================================

// Threepp クラス（定数と列挙型を保持）
class Threepp {
public:
	Threepp() {}
};

#define ENUM(n) Variant(#n, (int)n)
#define THREE_SUBCLASS(Class) NCB_SUBCLASS(Class, ThreeWrapper<Class>)
#define NCB_SUBCLASS_DELAY(Class) NCB_SUBCLASS(Class, Class)

NCB_REGISTER_CLASS(Threepp)
{
	// ====== Constants / Enums ======
	// Side
	ENUM(Side::Front);
	ENUM(Side::Back);
	ENUM(Side::Double);

	// Blending
	ENUM(Blending::None);
	ENUM(Blending::Normal);
	ENUM(Blending::Additive);
	ENUM(Blending::Subtractive);
	ENUM(Blending::Multiply);
	ENUM(Blending::Custom);

	// CullFace
	ENUM(CullFace::None);
	ENUM(CullFace::Back);
	ENUM(CullFace::Front);
	ENUM(CullFace::FrontBack);

	// ShadowMap
	ENUM(ShadowMap::Basic);
	ENUM(ShadowMap::PFC);
	ENUM(ShadowMap::PFCSoft);
	ENUM(ShadowMap::VSM);

	// ToneMapping
	ENUM(ToneMapping::None);
	ENUM(ToneMapping::Linear);
	ENUM(ToneMapping::Reinhard);
	ENUM(ToneMapping::Cineon);
	ENUM(ToneMapping::ACESFilmic);
	ENUM(ToneMapping::Custom);

	// ColorSpace (旧 Encoding。値は不変: Linear=3000, sRGB=3001, Gamma=3007)
	ENUM(ColorSpace::Linear);
	ENUM(ColorSpace::sRGB);
	ENUM(ColorSpace::Gamma);
	// 後方互換: 旧 Encoding:: 名も同じ値で残す
	Variant("Encoding::Linear", (int)ColorSpace::Linear);
	Variant("Encoding::sRGB",   (int)ColorSpace::sRGB);
	Variant("Encoding::Gamma",  (int)ColorSpace::Gamma);

	// TextureWrapping
	ENUM(TextureWrapping::Repeat);
	ENUM(TextureWrapping::ClampToEdge);
	ENUM(TextureWrapping::MirroredRepeat);

	// Filter
	ENUM(Filter::Nearest);
	ENUM(Filter::Linear);
	ENUM(Filter::NearestMipmapNearest);
	ENUM(Filter::NearestMipmapLinear);
	ENUM(Filter::LinearMipmapNearest);
	ENUM(Filter::LinearMipmapLinear);

	// Loop
	ENUM(Loop::Once);
	ENUM(Loop::Repeat);
	ENUM(Loop::PingPong);

	// ====== Math Classes ======
	NCB_SUBCLASS_DELAY(Vector2);
	NCB_SUBCLASS_DELAY(Vector3);
	NCB_SUBCLASS_DELAY(Color);
	NCB_SUBCLASS_DELAY(Euler);
	NCB_SUBCLASS_DELAY(Quaternion);
	NCB_SUBCLASS_DELAY(Matrix3);
	NCB_SUBCLASS_DELAY(Matrix4);
	NCB_SUBCLASS_DELAY(Box2);
	NCB_SUBCLASS_DELAY(Box3);
	NCB_SUBCLASS_DELAY(Sphere);
	NCB_SUBCLASS_DELAY(Ray);
	NCB_SUBCLASS_DELAY(Plane);
	NCB_SUBCLASS_DELAY(Line3);
	NCB_SUBCLASS_DELAY(Triangle);
	NCB_SUBCLASS_DELAY(Spherical);
	NCB_SUBCLASS_DELAY(Cylindrical);

	// ====== Scenes / Fog ======
	NCB_SUBCLASS_DELAY(Fog);
	NCB_SUBCLASS_DELAY(FogExp2);
	NCB_SUBCLASS_DELAY(Raycaster);

	// ====== Core Classes ======
	THREE_SUBCLASS(Object3D);
	THREE_SUBCLASS(BufferGeometry);
	THREE_SUBCLASS(Clock);

	// ====== Cameras ======
	THREE_SUBCLASS(Camera);
	THREE_SUBCLASS(PerspectiveCamera);
	THREE_SUBCLASS(OrthographicCamera);

	// ====== Scenes ======
	THREE_SUBCLASS(Scene);

	// ====== Lights ======
	THREE_SUBCLASS(Light);
	THREE_SUBCLASS(AmbientLight);
	THREE_SUBCLASS(DirectionalLight);
	THREE_SUBCLASS(PointLight);
	THREE_SUBCLASS(SpotLight);
	THREE_SUBCLASS(HemisphereLight);

	// ====== Materials ======
	THREE_SUBCLASS(Material);
	THREE_SUBCLASS(MeshBasicMaterial);
	THREE_SUBCLASS(MeshLambertMaterial);
	THREE_SUBCLASS(MeshPhongMaterial);
	THREE_SUBCLASS(MeshStandardMaterial);
	THREE_SUBCLASS(MeshNormalMaterial);
	THREE_SUBCLASS(MeshDepthMaterial);
	THREE_SUBCLASS(MeshToonMaterial);
	THREE_SUBCLASS(MeshMatcapMaterial);
	THREE_SUBCLASS(ShaderMaterial);
	THREE_SUBCLASS(RawShaderMaterial);
	THREE_SUBCLASS(LineBasicMaterial);
	THREE_SUBCLASS(PointsMaterial);
	THREE_SUBCLASS(SpriteMaterial);

	// ====== Geometries ======
	THREE_SUBCLASS(BoxGeometry);
	THREE_SUBCLASS(SphereGeometry);
	THREE_SUBCLASS(PlaneGeometry);
	THREE_SUBCLASS(CylinderGeometry);
	THREE_SUBCLASS(ConeGeometry);
	THREE_SUBCLASS(TorusGeometry);
	THREE_SUBCLASS(TorusKnotGeometry);
	THREE_SUBCLASS(CircleGeometry);
	THREE_SUBCLASS(RingGeometry);
	THREE_SUBCLASS(IcosahedronGeometry);
	THREE_SUBCLASS(OctahedronGeometry);
	THREE_SUBCLASS(CapsuleGeometry);
	THREE_SUBCLASS(LatheGeometry);
	THREE_SUBCLASS(EdgesGeometry);
	THREE_SUBCLASS(WireframeGeometry);

	// ====== Objects ======
	THREE_SUBCLASS(Group);
	THREE_SUBCLASS(Mesh);
	THREE_SUBCLASS(Line);
	THREE_SUBCLASS(LineSegments);
	THREE_SUBCLASS(Points);
	THREE_SUBCLASS(Sprite);
	THREE_SUBCLASS(InstancedMesh);
	THREE_SUBCLASS(LOD);
	THREE_SUBCLASS(Bone);
	THREE_SUBCLASS(Skeleton);
	THREE_SUBCLASS(SkinnedMesh);
	THREE_SUBCLASS(Sky);
	THREE_SUBCLASS(Water);
	THREE_SUBCLASS(Reflector);

	// ====== Helpers ======
	THREE_SUBCLASS(AxesHelper);
	THREE_SUBCLASS(GridHelper);
	THREE_SUBCLASS(ArrowHelper);
	THREE_SUBCLASS(BoxHelper);
	THREE_SUBCLASS(CameraHelper);
	THREE_SUBCLASS(DirectionalLightHelper);
	THREE_SUBCLASS(PointLightHelper);
	THREE_SUBCLASS(SpotLightHelper);
	THREE_SUBCLASS(PolarGridHelper);
	THREE_SUBCLASS(PlaneHelper);

	// ====== Textures ======
	THREE_SUBCLASS(Texture);
	THREE_SUBCLASS(DataTexture);

	// ====== Loaders ======
	NCB_SUBCLASS(TextureLoader, TextureLoaderWrapper);

	// ====== Renderers ======
	THREE_SUBCLASS(GLRenderer);

	// ====== VRM (Phase 0/1) ======
	THREE_SUBCLASS(VRMLoader);
	THREE_SUBCLASS(VRM);
	THREE_SUBCLASS(VRMAnimationLoader);
	THREE_SUBCLASS(VRMAnimation);

	// ====== 汎用 glTF/GLB ======
	THREE_SUBCLASS(GLTFLoader);
	THREE_SUBCLASS(GLTFModel);

	// ====== アニメーション (glTF 内蔵アニメ再生) ======
	THREE_SUBCLASS(ModelAnimator);
	THREE_SUBCLASS(AnimationAction);
	Variant("Loop_Once",     (int)Loop::Once);      // 1回で停止
	Variant("Loop_Repeat",   (int)Loop::Repeat);    // ループ (既定)
	Variant("Loop_PingPong", (int)Loop::PingPong);  // 往復

	// ====== Curves ======
	THREE_SUBCLASS(LineCurve);
	THREE_SUBCLASS(LineCurve3);
	THREE_SUBCLASS(QuadraticBezierCurve);
	THREE_SUBCLASS(QuadraticBezierCurve3);
	THREE_SUBCLASS(CubicBezierCurve);
	THREE_SUBCLASS(CubicBezierCurve3);
	THREE_SUBCLASS(SplineCurve);
	THREE_SUBCLASS(EllipseCurve);
	THREE_SUBCLASS(CatmullRomCurve3);
	THREE_SUBCLASS(Path);
	THREE_SUBCLASS(Shape);

	// ====== Shape-based Geometries ======
	THREE_SUBCLASS(ShapeGeometry);
	THREE_SUBCLASS(ExtrudeGeometry);

	// ====== CurveType Enum ======
	ENUM(CatmullRomCurve3::CurveType::centripetal);
	ENUM(CatmullRomCurve3::CurveType::chordal);
	ENUM(CatmullRomCurve3::CurveType::catmullrom);
}

// ----------------------------------- 起動・開放処理

NCB_PRE_REGIST_CALLBACK(initThreepp);
NCB_POST_UNREGIST_CALLBACK(deInitThreepp);