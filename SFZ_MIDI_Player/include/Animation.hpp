#pragma once
#include <Siv3D.hpp>

inline double RationalBezier(double x0, double x1, double x2, double w, double t)
{
	const double u = w;
	const double v = (1.0 - w) * 0.5;
	const double normalize = 1.0 / (v - 2.0 * t * (t - 1.0) * (u - v));

	return normalize * (v * (1 - t) * (1 - t) * x0 + u * 2 * (1 - t) * t * x1 + v * t * t * x2);
}

//https://twitter.com/agehama_/status/1454654881373909006
//https://www.desmos.com/calculator/uygzzf9x6r
inline double BlendRationalBezier(double w, double m, double t)
{
	if (t < m)
	{
		return RationalBezier(0, 0, m, w, Math::InvLerp(0.0, m, t));
	}
	else
	{
		return RationalBezier(m, 1, 1, w, Math::InvLerp(m, 1.0, t));
	}
}

//https://twitter.com/LucaRood/status/1406399062677540873
//https://www.desmos.com/calculator/5p7yrs762c
inline double BlendExp(double s, double a, double b, double t)
{
	if (t < a)
	{
		return std::pow(t / a, s / (b / a)) * b;
	}
	else
	{
		return 1.0 - std::pow((1.0 - t) / (1.0 - a), s / ((1.0 - b) / (1.0 - a))) * (1.0 - b);
	}
}

enum class InterpolateType
{
	RationalBezier,
	ExpBlend,
	Linear,

	Length
};

struct KeyFrame
{
	KeyFrame(double value) :
		value(value)
	{}

	KeyFrame(double value, double paramX, double paramY, double paramW, InterpolateType type) :
		value(value),
		paramX(paramX),
		paramY(paramY),
		paramW(paramW),
		type(type)
	{}

	double value;
	double paramX = 0.5;
	double paramY = 0.5;
	double paramW = 0.5;
	InterpolateType type = InterpolateType::RationalBezier;

	double interpolate(double t) const
	{
		switch (type)
		{
		case InterpolateType::RationalBezier:
		{
			return BlendRationalBezier(paramW, paramX, t);
		}
		case InterpolateType::ExpBlend:
		{
			return BlendExp(paramW, paramX, paramY, t);
		}
		case InterpolateType::Linear:
		{
			return t;
		}
		default:
			assert(false);
			return 0;
		}
	}
};

// Animation.hpp
