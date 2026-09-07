#pragma once
#include "Scene.h"
#include "Terrain.h"
#include "TerrainPipeline.h"

// 2번 샘플: 펄린 노이즈로 높이를 만든 지형.
// FlatGridScene 과 다른 점은 HeightField 를 MakePerlin 으로 굽는 것뿐이다.
class PerlinNoiseScene : public Scene
{
public:
	bool Initialize(GraphicsCore& core) override;
	void Render(GraphicsCore& core, const Camera& camera, bool wireframe) override;

	DirectX::XMFLOAT3 GetCameraStartPosition() const override { return { 0.0f, 120.0f, -260.0f }; }

private:
	static constexpr UINT GridResolution = 257;
	static constexpr float CellSize = 1.5f;

	// 주파수에 정수를 넣으면 모든 표본이 격자점에 떨어져 높이가 전부 0 이 된다.
	static constexpr float NoiseFrequency = 0.02f;
	static constexpr float NoiseAmplitude = 22.0f;
	static constexpr int NoiseOctaves = 5;
	static constexpr float HeightScale = 26.0f;

	TerrainPipeline m_pipeline;
	TerrainMesh m_mesh;
};
