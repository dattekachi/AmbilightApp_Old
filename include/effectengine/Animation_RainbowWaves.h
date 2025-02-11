#pragma once

#include <effectengine/AnimationBase.h>

#define ANIM_RAINBOW_WAVES "Rainbow waves"

class Animation_RainbowWaves : public AnimationBase
{
public:

	Animation_RainbowWaves(QString name = ANIM_RAINBOW_WAVES);

	void Init(
		QImage& ambilightImage,
		int ambilightLatchTime) override;

	bool Play(QPainter* painter) override;
	bool hasLedData(QVector<ColorRgb>& buffer) override;

	static EffectDefinition getDefinition();

protected:

	int		hue;
};
