#pragma once

// 프로젝트 전체가 쓰는 공용 헤더 모음.
// 여기에 모아두면 각 파일이 D3D12 헤더를 일일이 챙기지 않아도 된다.

#include<Windows.h>
#include<wrl.h> //ComPtr용 헤더파일

#include<d3d12.h>      // D3D12 핵심 인터페이스
#include<dxgi1_6.h>    // 어댑터 열거와 스왑체인
#include<dxcapi.h>     // 셰이더 컴파일러(DXC). 현재는 D3DCompile 을 쓰고 있어 예비용이다.

// Microsoft 가 배포하는 D3D12 도우미 헤더.
// CD3DX12_ 로 시작하는 구조체들이 장황한 설정 코드를 크게 줄여준다.
#include"d3dx12.h"

#include<DirectXMath.h>          // 벡터/행렬 연산
#include<DirectXPackedVector.h>  // 압축 벡터 형식

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "dxguid.lib")

// ComPtr 은 COM 객체의 참조 횟수를 자동으로 관리하는 스마트 포인터다.
// D3D12 객체는 전부 COM 이라 이걸로 잡아두면 해제를 잊을 일이 없다.
using namespace Microsoft::WRL;
