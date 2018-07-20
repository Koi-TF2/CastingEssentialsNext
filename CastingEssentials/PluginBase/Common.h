#pragma once
#include <Color.h>
#include <dbg.h>
#include <string>
#include <mathlib/mathlib.h>

#pragma warning(disable : 4355)    // 'this': used in base member initializer list
#pragma warning(disable : 4592)    // 'x': symbol will be dynamically initialized (implementation limitation)
#pragma warning(disable : 4533)    // initialization of 'x' is skipped by 'instruction' -- should only be a warning, but is promoted error for some reason?

//#define PLUGIN_VERSION "r7b1"

static constexpr const char* PLUGIN_NAME = "CastingEssentials";
extern const char* const PLUGIN_VERSION_ID;
extern const char* const PLUGIN_FULL_VERSION;

//#define NDEBUG_PER_FRAME_SUPPORT 1
#ifdef NDEBUG_PER_FRAME_SUPPORT
// Oddly specific
static constexpr float NDEBUG_PERSIST_TILL_NEXT_FRAME = -1.738f;
#else
static constexpr float NDEBUG_PERSIST_TILL_NEXT_FRAME = 0; // NDEBUG_PERSIST_TILL_NEXT_SERVER
#endif

#define VPROF_BUDGETGROUP_CE _T("CastingEssentials")

// For passing into strspn or whatever
static constexpr const char* WHITESPACE_CHARS = "\t\n\v\f\r ";

static constexpr const char* s_ObserverModes[] =
{
	"Unspecified",
	"Deathcam",
	"Freezecam",
	"Fixed",
	"In-eye",
	"Third-person chase",
	"Point of interest",
	"Free roaming",
};

static constexpr const char* s_ShortObserverModes[] =
{
	"Unspecified",
	"Deathcam",
	"Freezecam",
	"Fixed",
	"Firstperson",
	"Thirdperson",
	"POI",
	"Roaming",
};

class CCommand;
class ConVar;
class CSteamID;
class KeyValues;
class QAngle;
class Vector;

template<class... Parameters> __forceinline void PluginMsg(const char* fmt, Parameters... param)
{
	ConColorMsg(Color(0, 153, 153, 255), "[%s] ", PLUGIN_NAME);
	Msg(fmt, param...);
}
template<class... Parameters> __forceinline void PluginWarning(const char* fmt, const Parameters&... param)
{
	ConColorMsg(Color(0, 153, 153, 255), "[%s] ", PLUGIN_NAME);
	Warning(fmt, param...);
}
template<class... Parameters> __forceinline void PluginColorMsg(const Color& color, const char* fmt, Parameters... param)
{
	ConColorMsg(Color(0, 153, 153, 255), "[%s] ", PLUGIN_NAME);
	ConColorMsg(color, fmt, param...);
}

bool TryParseInteger(const char* str, int& out);
bool TryParseFloat(const char* str, float& out);

inline bool IsStringEmpty(const std::string_view& s) { return s.empty(); }
inline bool IsStringEmpty(const std::string* s)
{
	if (!s)
		return true;

	return s->empty();
}

inline std::string vstrprintf(const char* fmt, va_list args)
{
	va_list args2;
	va_copy(args2, args);

	const auto length = vsnprintf(nullptr, 0, fmt, args) + 1;

	Assert(length > 0);
	if (length <= 0)
		return nullptr;

	std::string retVal;
	retVal.resize(length);
	const auto written = vsnprintf(&retVal[0], length, fmt, args2);

	Assert((written + 1) == length);

	return retVal;
}
inline std::string strprintf(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	return vstrprintf(fmt, args);
}

inline const char* stristr(const char* const searchThis, const char* const forThis)
{
	// smh
	std::string lowerSearchThis(searchThis);
	for (auto& c : lowerSearchThis)
		c = (char)tolower(c);

	std::string lowerForThis(forThis);
	for (auto& c : lowerForThis)
		c = (char)tolower(c);

	auto const ptr = strstr(lowerSearchThis.c_str(), lowerForThis.c_str());
	if (!ptr)
		return nullptr;

	auto const dist = std::distance(lowerSearchThis.c_str(), ptr);

	return (const char*)(searchThis + dist);
}

// Easing functions, see https://www.desmos.com/calculator/wist7qm16z for live demo
inline float EaseOut(float x, float bias = 0.5)
{
	//Assert(x >= 0 && x <= 1);
	//Assert(bias >= 0 && bias <= 1);
	return 1 - std::pow(1 - std::pow(x, bias), 1 / bias);
}
inline float EaseIn(float x, float bias = 0.5)
{
	//Assert(x >= 0 && x <= 1);
	//Assert(bias >= 0 && bias <= 1);
	return std::pow(1 - std::pow(1 - x, bias), 1 / bias);
}
inline float EaseOut2(float x, float bias = 0.35)
{
	return 1 - std::pow(-x + 1, 1 / bias);
}
inline float EaseInSlope(float x, float bias = 0.5)
{
	return std::pow(1 - std::pow(1 - x, bias), (1 / bias) - 1) * std::pow(1 - x, bias - 1);
}
__forceinline float Bezier(float t, float x0, float x1, float x2)
{
	return Lerp(t, Lerp(t, x0, x1), Lerp(t, x1, x2));
}

inline float smoothstep(float x)
{
	return 3.0f*(x*x) - 2.0f*(x*x*x);
}
inline float smootherstep(float x)
{
	return 6.0f*(x*x*x*x*x) - 15.0f*(x*x*x*x) + 10.0f*(x*x*x);
}

extern std::string RenderSteamID(const CSteamID& id);

extern bool ReparseForSteamIDs(const CCommand& in, CCommand& out);

extern void SwapConVars(ConVar& var1, ConVar& var2);

extern Color ColorFromConVar(const ConVar& var, bool* success = nullptr);
extern Color ColorFromString(const char* str, bool* success = nullptr);
extern Vector ColorToVector(const Color& color);

extern bool ParseFloat3(const char* str, float& f1, float& f2, float& f3);
extern bool ParseVector(Vector& v, const char* str);
extern bool ParseAngle(QAngle& a, const char* str);

extern Vector GetViewOrigin();
extern int GetConLine();

extern std::string KeyValuesDumpAsString(KeyValues* kv, int indentLevel = 0);

Vector ApproachVector(const Vector& from, const Vector& to, float speed);

template <typename T, std::size_t N> constexpr std::size_t arraysize(T const (&)[N]) noexcept { return N; }

constexpr float Rad2Deg(float radians)
{
	return radians * float(180.0 / 3.14159265358979323846);
}
constexpr float Deg2Rad(float degrees)
{
	return degrees * float(3.14159265358979323846 / 180);
}

inline float UnscaleFOVByWidthRatio(float scaledFov, float ratio)
{
	return Rad2Deg(2 * std::atan(std::tan(Deg2Rad(scaledFov * 0.5f)) / ratio));
}
inline float UnscaleFOVByAspectRatio(float scaledFOV, float aspectRatio)
{
	return UnscaleFOVByWidthRatio(scaledFOV, aspectRatio / (4.0f / 3.0f));
}

template<typename T> constexpr T* FirstNotNull(T* first, T* second)
{
	return first ? first : second;
}
template<typename T> constexpr T* FirstNotNull(T* first, T* second, T* third)
{
	if (first)
		return first;
	if (second)
		return second;

	return third;
}

template<class T>
class VariablePusher final
{
public:
	VariablePusher() = delete;
	VariablePusher(const VariablePusher<T>& other) = delete;
	VariablePusher(VariablePusher<T>&& other)
	{
		m_Variable = std::move(other.m_Variable);
		other.m_Variable = nullptr;
		m_OldValue = std::move(other.m_OldValue);
	}
	VariablePusher(T& variable, const T& newValue) : m_Variable(&variable)
	{
		m_OldValue = std::move(*m_Variable);
		*m_Variable = std::move(newValue);
	}
	VariablePusher(T& variable, T&& newValue) : m_Variable(&variable)
	{
		m_OldValue = std::move(*m_Variable);
		*m_Variable = std::move(newValue);
	}
	~VariablePusher()
	{
		if (m_Variable)
			*m_Variable = std::move(m_OldValue);
	}

	T& GetOldValue() { return m_OldValue; }
	const T& GetOldValue() const { return m_OldValue; }

private:
	T* const m_Variable;
	T m_OldValue;
};

template<class T> inline VariablePusher<T> CreateVariablePusher(T& variable, T&& newValue)
{
	return VariablePusher<T>(variable, newValue);
}

// smh why were these omitted
namespace std
{
	inline std::string to_string(const char* str) { return std::string(str); }
}

template<size_t i> struct _PADDING_HELPER : _PADDING_HELPER<i - 1>
{
	static_assert(i != 0, "0 size struct not supported in C++");
	std::byte : 8;
};
template<> struct _PADDING_HELPER<1>
{
	std::byte : 8;
};
template<> struct _PADDING_HELPER<size_t(-1)>
{
	// To catch infinite recursion
};

#define PADDING(size) _PADDING_HELPER<size> EXPAND_CONCAT(CE_PADDING, __COUNTER__)