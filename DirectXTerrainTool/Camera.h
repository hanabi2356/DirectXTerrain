#pragma once
#include "Common.h"

// 지형을 둘러보기 위한 자유 시점 카메라.
// 마우스 오른쪽 버튼 드래그로 회전, WASD 로 이동, Q/E 로 상하 이동한다.
class Camera
{
public:
	void SetPerspective(float fovYRadians, float aspect, float nearZ, float farZ);
	void SetAspect(float aspect) { m_aspect = aspect; }
	void SetPosition(const DirectX::XMFLOAT3& position) { m_position = position; }
	void LookAt(const DirectX::XMFLOAT3& target);

	void Update(float deltaTime);
	void OnMouseMove(int x, int y);
	void EndDrag() { m_dragging = false; }

	DirectX::XMMATRIX GetView() const;
	DirectX::XMMATRIX GetProjection() const;
	DirectX::XMMATRIX GetViewProjection() const;

	DirectX::XMVECTOR GetForward() const;
	const DirectX::XMFLOAT3& GetPosition() const { return m_position; }

private:
	DirectX::XMFLOAT3 m_position = { 0.0f, 45.0f, -95.0f };
	float m_yaw = 0.0f;     // +Z 를 바라볼 때 0 (라디안)
	float m_pitch = 0.0f;   // 위를 볼 때 양수

	float m_fovY = DirectX::XM_PIDIV4;
	float m_aspect = 16.0f / 9.0f;
	float m_nearZ = 0.5f;
	float m_farZ = 2000.0f;

	float m_moveSpeed = 45.0f;
	float m_lookSpeed = 0.004f;

	bool m_dragging = false;
	int m_lastMouseX = 0;
	int m_lastMouseY = 0;
};
