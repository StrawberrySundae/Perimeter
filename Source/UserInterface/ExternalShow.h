#pragma once

void terExternalQuant();

void terExternalRegionShowUniform(Region* region,uint32_t color);
void terExternalRegionShowLine(Region* region,uint32_t diffuse);
void terExternalRegionShowColumn(Column* column,uint32_t color);
void terExternalRegionShowLineZeroplast(Region* region,uint32_t diffuse);
void terExternalRegionShowLineZeroplastVertical(Region* region,uint32_t diffuse);

void terCircleShowGraph(const Vect3f& pos,float r, const struct CircleColor& circleColor);
void terCircleShow(const Vect3f& pos0,const Vect3f& pos1,float r, const struct CircleColor& circleColor);
void terCircleShow(const Vect3f& pos0,const Vect3f& pos1,float r0, float r1, const struct CircleColor& circleColor);

class cCircleShow
{
public:
	cCircleShow();
	~cCircleShow();

	void Circle(const Vect3f& pos0,const Vect3f& pos1,float r0,float r1, const CircleColor& attr);
	void Circle(const Vect3f& pos,float r,const CircleColor& attr);
	void Show(int dotted);
	void Quant();

	void Lock();
	void Unlock();
	void Clear();

	//Функция terCircleShow вызывается без интерполяции и в графическом потоке
	void SetNoInterpolationLock(){Lock();no_interpolation=true;};
	void SetNoInterpolationUnlock(){no_interpolation=false;Unlock();};
protected:
	struct sCircle
	{
		Vect3f pos[2];
		float r[2];
		CircleColor attr;
	};
	struct sCircleGraph
	{
		Vect3f pos;
		float r;
		CircleColor attr;
	};

	struct sCircleType
	{
		std::vector<sCircle> circles;
		std::vector<sCircleGraph> circles_graph;
		cIUnkClass* external_show;
	};

	std::vector<sCircleType> types;
	float u_begin;

	void CircleShow(const Vect3f& pos,float r, const CircleColor& circleColor);

	bool no_interpolation;
	bool in_lock;//для assert
	MTDECLARE(lock);
};

extern cCircleShow* gbCircleShow;

class terRegionColumnMain
{
	std::vector<cObjectNodeRoot*> object;
public:
	terRegionColumnMain();
	virtual ~terRegionColumnMain();
	void quant();
protected:
	void clear();
};