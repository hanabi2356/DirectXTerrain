#pragma once
#include<Windows.h>
#include<wrl.h> //ComPtr용 헤더파일

#include<d3d12.h>
#include<dxgi1_6.h>
#include<dxcapi.h>


#include"d3dx12.h"
#include<DirectXMath.h>
#include<DirectXPackedVector.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "dxguid.lib")

using namespace Microsoft::WRL;
