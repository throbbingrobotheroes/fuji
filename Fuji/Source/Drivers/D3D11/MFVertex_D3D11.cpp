#include "Fuji.h"

#if MF_RENDERER == MF_DRIVER_D3D11 || defined(MF_RENDERPLUGIN_D3D11)

#if defined(MF_RENDERPLUGIN_D3D11)
	#define MFVertex_InitModulePlatformSpecific MFVertex_InitModulePlatformSpecific_D3D11
	#define MFVertex_DeinitModulePlatformSpecific MFVertex_DeinitModulePlatformSpecific_D3D11
	#define MFVertex_CreateVertexDeclarationPlatformSpecific MFVertex_CreateVertexDeclarationPlatformSpecific_D3D11
	#define MFVertex_DestroyVertexDeclarationPlatformSpecific MFVertex_DestroyVertexDeclarationPlatformSpecific_D3D11
	#define MFVertex_CreateVertexBufferPlatformSpecific MFVertex_CreateVertexBufferPlatformSpecific_D3D11
	#define MFVertex_DestroyVertexBufferPlatformSpecific MFVertex_DestroyVertexBufferPlatformSpecific_D3D11
	#define MFVertex_LockVertexBuffer MFVertex_LockVertexBuffer_D3D11
	#define MFVertex_UnlockVertexBuffer MFVertex_UnlockVertexBuffer_D3D11
	#define MFVertex_CreateIndexBufferPlatformSpecific MFVertex_CreateIndexBufferPlatformSpecific_D3D11
	#define MFVertex_DestroyIndexBufferPlatformSpecific MFVertex_DestroyIndexBufferPlatformSpecific_D3D11
	#define MFVertex_LockIndexBuffer MFVertex_LockIndexBuffer_D3D11
	#define MFVertex_UnlockIndexBuffer MFVertex_UnlockIndexBuffer_D3D11
	#define MFVertex_SetVertexDeclaration MFVertex_SetVertexDeclaration_D3D11
	#define MFVertex_SetVertexStreamSource MFVertex_SetVertexStreamSource_D3D11
	#define MFVertex_SetIndexBuffer MFVertex_SetIndexBuffer_D3D11
	#define MFVertex_RenderVertices MFVertex_RenderVertices_D3D11
	#define MFVertex_RenderIndexedVertices MFVertex_RenderIndexedVertices_D3D11
#endif

#include "MFVector.h"
#include "MFHeap.h"
#include "MFVertex_Internal.h"
#include "MFDebug.h"
#include "MFMesh_Internal.h"
#include "MFRenderer_D3D11.h"

//---------------------------------------------------------------------------------------------------------------------
DXGI_FORMAT MFRenderer_D3D11_GetFormat(MFVertexDataFormat format);
const char* MFRenderer_D3D11_GetSemanticName(MFVertexElementType type);
//---------------------------------------------------------------------------------------------------------------------
extern int gVertexDataStride[MFVDF_Max];
//---------------------------------------------------------------------------------------------------------------------
extern const uint8 *g_pVertexShaderData;
extern uint32 g_vertexShaderSize;
//---------------------------------------------------------------------------------------------------------------------
extern ID3D11Device* g_pd3dDevice;
extern ID3D11DeviceContext* g_pImmediateContext;
//---------------------------------------------------------------------------------------------------------------------
static const D3D11_PRIMITIVE_TOPOLOGY gPrimTopology[MFPT_Max] =
{
	D3D11_PRIMITIVE_TOPOLOGY_POINTLIST,		// MFVPT_Points
	D3D11_PRIMITIVE_TOPOLOGY_LINELIST,		// MFVPT_LineList
	D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP,		// MFVPT_LineStrip
	D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,	// MFVPT_TriangleList
	D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,	// MFVPT_TriangleStrip
	D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED,		// MFVPT_TriangleFan
};
//---------------------------------------------------------------------------------------------------------------------
static const D3D11_PRIMITIVE gPrimType[MFPT_Max] =
{
	D3D11_PRIMITIVE_POINT,		// MFVPT_Points
	D3D11_PRIMITIVE_LINE,		// MFVPT_LineList
	D3D11_PRIMITIVE_LINE,		// MFVPT_LineStrip
	D3D11_PRIMITIVE_TRIANGLE,	// MFVPT_TriangleList
	D3D11_PRIMITIVE_TRIANGLE,	// MFVPT_TriangleStrip
	D3D11_PRIMITIVE_UNDEFINED,	// MFVPT_TriangleFan
};
//---------------------------------------------------------------------------------------------------------------------
MFVertexDataFormat MFVertexD3D11_ChoooseDataType(MFVertexElementType elementType, int components)
{
	MFDebug_Assert((components >= 0) && (components <= 4), "Invalid number of components");

	const MFVertexDataFormat floatComponents[5] = { MFVDF_Unknown, MFVDF_Float1, MFVDF_Float2, MFVDF_Float3, MFVDF_Float4 };
	switch(elementType)
	{
		case MFVET_Colour:
		case MFVET_Weights:
			return MFVDF_UByte4N_RGBA;
		case MFVET_Indices:
			return MFVDF_UByte4_RGBA;
		default:
			break;
	}
	// everything else is a float for now...
	return floatComponents[components];
}
//---------------------------------------------------------------------------------------------------------------------
void MFVertex_InitModulePlatformSpecific()
{
}
//---------------------------------------------------------------------------------------------------------------------
void MFVertex_DeinitModulePlatformSpecific()
{
}
//---------------------------------------------------------------------------------------------------------------------
bool MFVertex_CreateVertexDeclarationPlatformSpecific(MFVertexDeclaration *pDeclaration)
{
	MFVertexElement *pElements = pDeclaration->pElements;
	MFVertexElementData *pElementData = pDeclaration->pElementData;

	int streamOffsets[16];
	MFZeroMemory(streamOffsets, sizeof(streamOffsets));

	D3D11_INPUT_ELEMENT_DESC elements[32];
	for(int a=0; a<pDeclaration->numElements; ++a)
	{
		MFVertexDataFormat dataFormat;
		if(pElements[a].format == MFVDF_Auto)
			dataFormat = MFVertexD3D11_ChoooseDataType(pElements[a].type, pElements[a].componentCount);
		else
			dataFormat = pElements[a].format;
		MFDebug_Assert(MFRenderer_D3D11_GetFormat(dataFormat) != (DXGI_FORMAT)-1, "Invalid vertex data format!");

		elements[a].SemanticName = MFRenderer_D3D11_GetSemanticName(pElements[a].type);
		elements[a].SemanticIndex = pElements[a].index;
		elements[a].Format = MFRenderer_D3D11_GetFormat(dataFormat);
		elements[a].InputSlot = pElements[a].stream;
		elements[a].AlignedByteOffset = streamOffsets[pElements[a].stream];
		elements[a].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		elements[a].InstanceDataStepRate = 0;

		pElementData[a].format = dataFormat;
		pElementData[a].offset = streamOffsets[pElements[a].stream];
		pElementData[a].stride = 0;
		pElementData[a].pData = NULL;

		streamOffsets[pElements[a].stream] += gVertexDataStride[dataFormat];
		pDeclaration->streamsUsed |= MFBIT(pElements[a].stream);
	}

	// set the strides for each component
	for (int a=0; a<pDeclaration->numElements; ++a)
		pElementData[a].stride = streamOffsets[pElements[a].stream];
	
	// this needs the vertex shader
	ID3D11InputLayout* pVertexLayout = NULL;
	HRESULT hr = g_pd3dDevice->CreateInputLayout(elements, pDeclaration->numElements, g_pVertexShaderData, g_vertexShaderSize, &pVertexLayout);
	if (FAILED(hr))
		return false;

	pDeclaration->pPlatformData = pVertexLayout;

	return true;
}
//---------------------------------------------------------------------------------------------------------------------
void MFVertex_DestroyVertexDeclarationPlatformSpecific(MFVertexDeclaration *pDeclaration)
{
	ID3D11InputLayout *pVertexLayout = (ID3D11InputLayout*)pDeclaration->pPlatformData;
	pVertexLayout->Release();
}
//---------------------------------------------------------------------------------------------------------------------
bool MFVertex_CreateVertexBufferPlatformSpecific(MFVertexBuffer *pVertexBuffer, void *pVertexBufferMemory)
{
    D3D11_BUFFER_DESC bd;
    MFZeroMemory(&bd, sizeof(bd));
	bd.Usage = (pVertexBuffer->type == MFVBType_Static) ? D3D11_USAGE_DEFAULT : D3D11_USAGE_DYNAMIC;
	bd.ByteWidth = pVertexBuffer->pVertexDeclatation->pElementData[0].stride * pVertexBuffer->numVerts;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = (pVertexBuffer->type == MFVBType_Static) ? 0 : D3D11_CPU_ACCESS_WRITE;

    D3D11_SUBRESOURCE_DATA InitData;
    MFZeroMemory(&InitData, sizeof(InitData));
    InitData.pSysMem = pVertexBufferMemory;
	
	ID3D11Buffer* pVB = NULL;
    HRESULT hr = g_pd3dDevice->CreateBuffer(&bd, pVertexBufferMemory ? &InitData : NULL, &pVB);
    if(FAILED(hr))
        return false;
	
	pVertexBuffer->pPlatformData = pVB;

	return true;
}
//---------------------------------------------------------------------------------------------------------------------
void MFVertex_DestroyVertexBufferPlatformSpecific(MFVertexBuffer *pVertexBuffer)
{
	ID3D11Buffer *pVB = (ID3D11Buffer*)pVertexBuffer->pPlatformData;
	pVB->Release();
}
//---------------------------------------------------------------------------------------------------------------------
MF_API void MFVertex_LockVertexBuffer(MFVertexBuffer *pVertexBuffer, void **ppVertices)
{
	MFDebug_Assert(pVertexBuffer, "Null vertex buffer");
	MFDebug_Assert(!pVertexBuffer->bLocked, "Vertex buffer already locked!");

	ID3D11Buffer *pVB = (ID3D11Buffer*)pVertexBuffer->pPlatformData;
	D3D11_MAPPED_SUBRESOURCE subresource;

	// SJS need to use D3D11_MAP_WRITE_NO_OVERWRITE some time
	D3D11_MAP map = (pVertexBuffer->bufferType == MFVBType_Static) ? D3D11_MAP_WRITE : D3D11_MAP_WRITE_DISCARD;

	//HRESULT hr = g_pImmediateContext->Map(pVB, 0, map, D3D11_MAP_FLAG_DO_NOT_WAIT, &subresource);

	//if (hr == DXGI_ERROR_WAS_STILL_DRAWING)
	//{
	//	MFDebug_Message("waiting on vertex buffer lock");

	//	hr = g_pImmediateContext->Map(pVB, 0, map, 0, &subresource);
	//}

	HRESULT hr = g_pImmediateContext->Map(pVB, 0, map, 0, &subresource);

	MFDebug_Assert(SUCCEEDED(hr), "Failed to map vertex buffer");

	if(ppVertices)
		*ppVertices = subresource.pData;

	for(int a=0; a<pVertexBuffer->pVertexDeclatation->numElements; ++a)
	{
		if(pVertexBuffer->pVertexDeclatation->pElements[a].stream == 0)
			pVertexBuffer->pVertexDeclatation->pElementData[a].pData = (char*)subresource.pData + pVertexBuffer->pVertexDeclatation->pElementData[a].offset;
		else
			pVertexBuffer->pVertexDeclatation->pElementData[a].pData = NULL;
	}

	pVertexBuffer->bLocked = true;
}
//---------------------------------------------------------------------------------------------------------------------
MF_API void MFVertex_UnlockVertexBuffer(MFVertexBuffer *pVertexBuffer)
{
	MFDebug_Assert(pVertexBuffer, "Null vertex buffer");
	ID3D11Buffer *pVB = (ID3D11Buffer*)pVertexBuffer->pPlatformData;
	g_pImmediateContext->Unmap(pVB, 0);
	pVertexBuffer->bLocked = false;
}
//---------------------------------------------------------------------------------------------------------------------
bool MFVertex_CreateIndexBufferPlatformSpecific(MFIndexBuffer *pIndexBuffer, uint16 *pIndexBufferMemory)
{
    D3D11_BUFFER_DESC bd;
    MFZeroMemory(&bd, sizeof(bd));
	
    bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(WORD) * pIndexBuffer->numIndices;
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bd.CPUAccessFlags = 0;
	
    D3D11_SUBRESOURCE_DATA InitData;
    MFZeroMemory(&InitData, sizeof(InitData));
    InitData.pSysMem = pIndexBufferMemory;

	ID3D11Buffer *pIB = NULL;
    HRESULT hr = g_pd3dDevice->CreateBuffer(&bd, &InitData, &pIB);
    if (FAILED(hr))
        return false;

	pIndexBuffer->pPlatformData = pIB;

	return true;
}
//---------------------------------------------------------------------------------------------------------------------
void MFVertex_DestroyIndexBufferPlatformSpecific(MFIndexBuffer *pIndexBuffer)
{
	ID3D11Buffer *pIB = (ID3D11Buffer*)pIndexBuffer->pPlatformData;
	pIB->Release();
}
//---------------------------------------------------------------------------------------------------------------------
MF_API void MFVertex_LockIndexBuffer(MFIndexBuffer *pIndexBuffer, uint16 **ppIndices)
{
	MFDebug_Assert(pIndexBuffer, "Null index buffer");
	MFDebug_Assert(!pIndexBuffer->bLocked, "Index buffer already locked!");

	*ppIndices = pIndexBuffer->pIndices;

	pIndexBuffer->bLocked = true;
}
//---------------------------------------------------------------------------------------------------------------------
MF_API void MFVertex_UnlockIndexBuffer(MFIndexBuffer *pIndexBuffer)
{
	MFDebug_Assert(pIndexBuffer, "Null index buffer");
	MFDebug_Assert(pIndexBuffer->bLocked, "Index buffer already locked!");

	ID3D11Buffer *pIB = (ID3D11Buffer*)pIndexBuffer->pPlatformData;
	
	D3D11_MAPPED_SUBRESOURCE subresource;

	D3D11_MAP map = D3D11_MAP_WRITE_DISCARD;
	
	HRESULT hr = g_pImmediateContext->Map(pIB, 0, map, D3D11_MAP_FLAG_DO_NOT_WAIT, &subresource);

	if (hr == DXGI_ERROR_WAS_STILL_DRAWING)
	{
		MFDebug_Message("waiting on index buffer lock");

		hr = g_pImmediateContext->Map(pIB, 0, map, 0, &subresource);
	}

	MFCopyMemory(subresource.pData, pIndexBuffer->pIndices, sizeof(uint16) * pIndexBuffer->numIndices);

	g_pImmediateContext->Unmap(pIB, 0);

	pIndexBuffer->bLocked = false;
}
//---------------------------------------------------------------------------------------------------------------------
MF_API void MFVertex_SetVertexDeclaration(MFVertexDeclaration *pVertexDeclaration)
{
	ID3D11InputLayout *pVertexLayout = pVertexDeclaration ? (ID3D11InputLayout*)pVertexDeclaration->pPlatformData : NULL;
    g_pImmediateContext->IASetInputLayout(pVertexLayout);
}
//---------------------------------------------------------------------------------------------------------------------
MF_API void MFVertex_SetVertexStreamSource(int stream, MFVertexBuffer *pVertexBuffer)
{
	MFDebug_Assert(pVertexBuffer, "Null vertex buffer");

	ID3D11Buffer *pVB = (ID3D11Buffer*)pVertexBuffer->pPlatformData;
    // Set vertex buffer
	UINT stride = pVertexBuffer->pVertexDeclatation->pElementData[0].stride;
    UINT offset = 0;
    g_pImmediateContext->IASetVertexBuffers(stream, 1, &pVB, &stride, &offset);

	//if (stream == 0)
	//	MFVertex_SetVertexDeclaration(pVertexBuffer->pVertexDeclatation);
}
//---------------------------------------------------------------------------------------------------------------------
MF_API void MFVertex_SetIndexBuffer(MFIndexBuffer *pIndexBuffer)
{
	ID3D11Buffer *pIB = (ID3D11Buffer*)pIndexBuffer->pPlatformData;
	g_pImmediateContext->IASetIndexBuffer(pIB, DXGI_FORMAT_R16_UINT, 0);
}
//---------------------------------------------------------------------------------------------------------------------
MF_API void MFVertex_RenderVertices(MFPrimType primType, int firstVertex, int numVertices)
{
    g_pImmediateContext->IASetPrimitiveTopology(gPrimTopology[primType]);
	g_pImmediateContext->Draw(numVertices, firstVertex);
}
//---------------------------------------------------------------------------------------------------------------------
MF_API void MFVertex_RenderIndexedVertices(MFPrimType primType, int vertexOffset, int indexOffset, int numVertices, int numIndices)
{
    g_pImmediateContext->IASetPrimitiveTopology(gPrimTopology[primType]);
    g_pImmediateContext->DrawIndexed(numIndices, indexOffset, vertexOffset);
}
//---------------------------------------------------------------------------------------------------------------------

#endif // MF_RENDERER
