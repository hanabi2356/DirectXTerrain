#pragma once
#include "Common.h"

class GraphicsCore;
class Camera;

// 메뉴 항목과 1:1 로 대응하는 샘플 식별자.
enum class SampleId
{
	None = 0,
	FlatGrid,
	PerlinNoise,
	HeightMap,
	TextureSplatting,
	QuadTreeCulling,
	DistanceLod1,
	DistanceLod2,
	Tessellation,
	SkyDome,
	PerturbedClouds,
	InfiniteChunks
};

// 샘플 하나가 구현해야 하는 최소 인터페이스.
// 리소스 생성은 Initialize 에서 끝내고, Render 는 커맨드 리스트 기록만 한다.
class Scene
{
public:
	virtual ~Scene() = default;

	virtual bool Initialize(GraphicsCore& core) = 0;
	virtual void Update(float /*deltaTime*/) {}
	virtual void Render(GraphicsCore& core, const Camera& camera, bool wireframe) = 0;
};
