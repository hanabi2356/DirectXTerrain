#pragma once
#include "Scene.h"

#include <memory>

class GraphicsCore;
class Camera;

// SampleId 와 실제 Scene 구현을 잇고, 전환 시 리소스 수명을 책임진다.
class SceneManager
{
public:
	// 아직 구현되지 않은 샘플이면 false 를 돌려주고 씬을 비워둔다.
	bool Switch(GraphicsCore& core, SampleId id);
	void Clear(GraphicsCore& core);

	void Update(float deltaTime);
	void Render(GraphicsCore& core, const Camera& camera, bool wireframe);

	bool HasScene() const { return m_scene != nullptr; }
	SampleId GetCurrent() const { return m_current; }

	DirectX::XMFLOAT3 GetCameraStartPosition() const;
	DirectX::XMFLOAT3 GetCameraStartTarget() const;

private:
	static std::unique_ptr<Scene> Create(SampleId id);

	std::unique_ptr<Scene> m_scene;
	SampleId m_current = SampleId::None;
};
