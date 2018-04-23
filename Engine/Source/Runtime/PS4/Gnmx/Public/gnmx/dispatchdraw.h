/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef _SCE_GNMX_DISPATCHDRAW_H
#define _SCE_GNMX_DISPATCHDRAW_H

#include <string.h>
#include <gnm/common.h>
#include <gnm/buffer.h>
#include <gnm/constants.h>
#include <gnmx/common.h>
#include <gnmx/error_gen.h>

#define ENABLE_NEW_DISPATCH_DRAW_DATA_FORMAT 1

namespace sce
{
	namespace Gnmx
	{
		const uint32_t kDispatchDrawIndexRingBufferMinimumSizeInIndices = 513*9;	///< The minimum size of the dispatch draw Index Ring Buffer (IRB) in indices that standard dispatch draw triangle culling requires to function; to operate correctly, the IRB must be guaranteed to have space for at least 3 compute thread groups to output maximum size output.
		const uint32_t kDispatchDrawVertexRingBufferMinimumSizeInVertices = 256*3;	///< The minimum size of the dispatch draw Vertex Ring Buffer (VRB) in vertices that standard dispatch draw triangle culling with VRB requires to function; to operate correctly, the VRB must be guaranteed to have space for at least 3 compute thread groups to output maximum size output.

		const uint16_t kDispatchDrawTriangleCullIndexDataMagic = 0x6e68;
		const uint8_t kDispatchDrawTriangleCullIndexDataVersionMajor = 1;
		const uint8_t kDispatchDrawTriangleCullIndexDataVersionMinor = 0;

		/** @brief Describes index data converted for use by a triangle culling shader with metadata required to draw it.
		*/
		class SCE_GNMX_EXPORT DispatchDrawTriangleCullIndexData
		{
		public:
			uint16_t		m_magic;					///< This is always set to kDispatchDrawTriangleCullIndexDataMagic.
			uint8_t			m_versionMajor;				///< The major version of this structure (kDispatchDrawTriangleCullIndexDataVersionMajor).
			uint8_t			m_versionMinor;				///< The minor version of this structure (kDispatchDrawTriangleCullIndexDataVersionMinor).
			uint32_t		m_numIndexDataBlocks;		///< The number of blocks encoded in <c><i>m_bufferInputIndexData</i></c>.
			uint8_t			m_numIndexBits;				///< The smallest value [1:16] such that <c>maxIndex < (1<<m_indexNumBits)</c>. This may always be set to 16 if instancing is not used.
			uint8_t			m_numIndexSpaceBits;		///< The largest value [0:m_numIndexBits-1] such that <c>maxIndex < (0xFFFF & (0xFFFF<<m_numIndexSpaceBits))</c>.
			uint8_t			m_numInstancesPerTgMinus1;	///< The sets the number of instances which will be evaluated per thread group minus 1.
			uint8_t			m_reserved;					///< A reserved value.
			uint32_t		m_reserved2;				///< A reserved value.
			Gnm::Buffer		m_bufferInputIndexData;		///< The input index data. This must be <c>Gnm::Buffer::initAsByteBuffer(pInputData, sizeofInputData)</c>.
		};

		const uint32_t kDispatchDrawClipCullFlagClipSpaceDX =	0x0000;	///< Clip space is  0 < Z < W.
		const uint32_t kDispatchDrawClipCullFlagClipSpaceOGL =	0x0001;	///< Clip space is -W < Z < W.
		const uint32_t kDispatchDrawClipCullFlagCullCW =		0x0002;	///< Cull clockwise triangles.
		const uint32_t kDispatchDrawClipCullFlagCullCCW =		0x0004;	///< Cull counter-clockwise triangles.
		const uint32_t kDispatchDrawClipCullMask =				0x0007; ///< The clip cull mask.

		/** @brief Describes the input data and settings for a dispatch draw triangle culling shader without vertex ring buffer usage.
		*/
		class SCE_GNMX_EXPORT DispatchDrawTriangleCullData
		{
		public:
			Gnm::Buffer		m_bufferIrb;			///< Index ring buffer. For 16 bit indices, this must be <c>Gnm::Buffer::initAsDataBuffer(pIrb, kDataFormatR16Uint, sizeofIrbInIndices)</c>, where <c><i>sizeofIrbInIndices</i></c> must be a multiple of 128 matching the value passed to setupDispatchDrawRingBuffers().
			Gnm::Buffer		m_bufferInputIndexData;	///< Input index data. Must be <c>Gnm::Buffer::initAsByteBuffer(pInputData, sizeofInputData)</c>.
			uint16_t		m_numIndexDataBlocks;	///< Number of index data blocks in <c><i>m_bufferInputIndexData</i></c>.
			uint16_t		m_gdsOffsetOfIrbWptr;	///< Offset in GDS of index ring buffer write pointer counter. Must match the value passed to Gnm::DispatchCommandBuffer::setupDispatchDrawIndexRingBuffer()
			uint32_t		m_sizeofIrbInIndices;	///< Size of index ring buffer in indices. Must match the value passed to Gnm::DispatchCommandBuffer::setupDispatchDrawIndexRingBuffer().

			uint16_t m_clipCullSettings;	///< Union of <c>kDispatchDrawClipCullFlag*</c> variable values.
				//Instancing support includes support for setNumInstances and setInstanceStepRate values up to 65536 only.
				// A single dispatchDraw supports as many instances as can be encoded to the IRB:
				//   The IRB requires (instance - m_firstInstance) and index to be packed into a single 16-bit value. We do this by storing ((instance - firstInstance)<<m_indexNumBits)|index.
				//     As a result, a single dispatchDraw call can only render a maximum of maxInstancesPerCall = (1<<(16 - m_numIndexBits)) instances.
				// Each dispatch draw thread group supports:
				//   A single instance of a single block from a multi-block draw,
				//   OR as many instances of a single block as will fit in a thread group of 256 threads.
				//     We can fit min(256/numVtx, 512/numTri) instances into a single thread group.
				//	   In this case, the vertex phase runs {vVtx = v_thread_id % numVtx, vInstance = v_thread_id / numVtx if (vInstance < numInstances) } 
				//	   and the triangle phase runs for (i=0;i<2;++i) { vTri = (v_thread_id + i*256) % numTri, vInstance = (v_thread_id + i*256) / numTri if (vInstance < numInstances) }.
				// A single dispatchDraw call will create (numInstancesPerCall / numInstancesPerTg)*m_numIndexDataBlocks wavefronts.
				// Gnmx can submit ((numInstancesTotal + maxInstancesPerCall-1)/maxInstancesPerCall) dispatchDraw calls to implement instancing
				// with up to 65536 instances, passing firstInstance, numInstances to each call in kShaderInputUsageImmDispatchDrawInstances.
			
			uint8_t  m_numIndexBits;				///< The number of index bits. This should be set to the smallest value [1:16] such that <c>maxIndex < (1<<m_indexNumBits)</c>. It may always be set to 16 if instancing is disabled (i.e. if <c>m_numInstancesMinus1 == 0</c>).
			uint8_t  m_numInstancesPerTgMinus1;		///< The number of instances which will be evaluated per thread group minus 1.
			
			//Partial draws are only supported at the block level, currently:
			uint16_t m_firstIndexDataBlock;			///< The render blocks <c>[m_firstIndexDataBlock:m_firstIndexDataBlock+m_numIndexDataBlocks-1]</c> in this dispatchDraw() call.
			uint16_t m_reserved;					///< Unused.
			
			/** Calculating area = <c>(x1/w1 - x2/w2)*y0/w0 + (x2/w2 - x0/w0)*y1/w1 + (x0/w0 - x1/w1)*y2/w2</c>,
				requires calculating the error that will be introduced by <c>*1/w</c> and quantization to hardware screenspace coordinates
				and only cull triangles with area less than <c>-max_area_error (CCW)</c> or area greater than <c>max_area_error (CW)</c>. */
			float m_quantErrorScreenX;
			float m_quantErrorScreenY; ///< See description for #m_quantErrorScreenX.
			/** Triangles which are clipped disable area culling to be safe, as clipping might add difficult to predict errors.
			    Clip if X is greater than <c><i>m_gbHorizClipAdjust</i> * W</c> or X less than <c>-<i>m_gbHorizClipAdjust</i> * W</c>. 
				Clip if Y is greater than <c><i>m_gbVertClipAdjust</i> * W</c> or <c>Y < -<i>m_gbVertClipAdjust</i> * W</c>. 
				Clip if Z is greater than <c>W</c> or Z less than <c>-(clip_z_gl ? 1.0 : 0.0) * W</c>.*/
			float m_gbHorizClipAdjust;
			float m_gbVertClipAdjust; ///< See description for #m_gbHorizClipAdjust.

			uint16_t m_instanceStepRate0Minus1;	// vInstanceOverStepRate0 = vInstance / (1 + m_instanceStepRate0Minus1)
			uint16_t m_instanceStepRate1Minus1;	// vInstanceOverStepRate1 = vInstance / (1 + m_instanceStepRate1Minus1)
		};

		/** @brief Describes the input data and settings for a dispatch draw triangle culling shader with vertex ring buffer usage.
		*/
		class SCE_GNMX_EXPORT DispatchDrawVrbTriangleCullData : public DispatchDrawTriangleCullData
		{
		public:
			Gnm::Buffer		m_bufferVrb;			///< The vertex ring buffer. This must be <c>Gnm::Buffer::initAsDataBuffer(pVrb, kDataFormatR32Uint, sizeofVrbInDwords)</c>, where <c><i>sizeofVrbInDwords</i></c> must be a power of 2.
		};
		
		/** @brief Describes the shared configuration data for a version 2 dispatch draw triangle culling shader without vertex ring buffer usage.
		*/
		class SCE_GNMX_EXPORT DispatchDrawTriangleCullV1SharedData
		{
		public:
			Gnm::Buffer m_bufferIrb;
			uint16_t m_gdsOffsetOfIrbWptr;
			uint16_t m_cullSettings;	//bit 0: clip_z_gl=1: Z < -W =0: Z < 0; bit 1: cull CW tris; bit 2: cull CCW tris; bits [3:15]: reserved
				//When calculating area = (x1/w1 - x2/w2)*y0/w0 + (x2/w2 - x0/w0)*y1/w1 + (x0/w0 - x1/w1)*y2/w2,
				// we must also calculate the error which will be introduced by *1/w and quantization to hardware screenspace coordinates
				// and only cull triangles with area < -max_area_error (CCW) or area > max_area_error (CW).
			float m_quantErrorScreenX;
			float m_quantErrorScreenY;
				//Triangles which are clipped disable area culling to be safe:
			float m_gbHorizClipAdjust;	// clip if X > m_gbHorizClipAdjust*W, X < -m_gbHorizClipAdjust*W
			float m_gbVertClipAdjust;	// clip if Y > m_gbVertClipAdjust*W, Y < -m_gbVertClipAdjust*W
										// clip if Z > W, or Z < -(clip_z_gl ? 1.0 : 0.0)*W
		};

		/** @brief Describes the shared configuration data for a version 2 dispatch draw triangle culling shader with vertex ring buffer usage.
		*	size = 13 <c>DWORD</c>s
		*/
		class SCE_GNMX_EXPORT DispatchDrawTriangleCullV1SharedDataWithVrb : public DispatchDrawTriangleCullV1SharedData
		{
		public:
			Gnm::Buffer m_bufferVrb;
		};

		/** @brief Describes the per dispatchDraw() call data for a version 2 dispatch draw triangle culling shader.
		*	size = 7 <c>DWORD</c>s with instancing disabled and 8 <c>DWORD</c>s with instancing enabled
		*/
		class SCE_GNMX_EXPORT DispatchDrawTriangleCullV1Data
		{
		public:
			DispatchDrawTriangleCullV1SharedData const *m_pShared;	///< A pointer to data shared between many draw calls including ring buffer configuration and clipping/culling state.
			Gnm::Buffer m_bufferInputIndexData;	///< The input index data. This must be <c>Gnm::Buffer::initAsByteBuffer(pInputData, sizeofInputData)</c>.
			uint16_t m_numIndexDataBlocks;		///< The number of index data blocks in <c><i>m_bufferInputIndexData</i></c>.
			uint8_t  m_numIndexBits;			///<(instancing only) The number of index bits. This should be set to the smallest value [1:16] such that <c>maxIndex < (1<<m_indexNumBits)</c>. It may always be set to 16 if instancing is disabled (i.e. if <c>numInstancesMinus1 == 0</c>), or to 0 for non-instancing shaders (those without kShaderInputUsageImmDispatchDrawInstances user SGPR).
			uint8_t  m_numInstancesPerTgMinus1;	///<(instancing only) The number of instances which will be evaluated per thread group minus 1. It may always be set to 0 if instancing is disabled.
			uint16_t m_instanceStepRate0Minus1;	///<(instancing only) <c>vInstanceOverStepRate0 = vInstance / (1 + m_instanceStepRate0Minus1)</c>
			uint16_t m_instanceStepRate1Minus1;	///<(instancing only) <c>vInstanceOverStepRate1 = vInstance / (1 + m_instanceStepRate1Minus1)</c>
		};
		static const size_t kSizeofDispatchDrawTriangleCullV1DataInstancingEnabled = sizeof(DispatchDrawTriangleCullV1Data);
		static const size_t kSizeofDispatchDrawTriangleCullV1DataInstancingDisabled = sizeof(DispatchDrawTriangleCullV1Data) - sizeof(uint32_t);

		/** @brief Enumerates special return values returned by getSizeofDispatchDrawInputData() or createDispatchDrawInputData(). */
		typedef enum DispatchDrawStatus
		{
			kDispatchDrawErrorInvalidArguments =			SCE_GNMX_ERROR_DISPATCH_DRAW_INVALID_ARGUMENTS,				///< Buffer creation failed because one or more of the input arguments were not valid.
			kDispatchDrawErrorOutOfSpaceForIndexData =		SCE_GNMX_ERROR_DISPATCH_DRAW_OUT_OF_SPACE_FOR_INDEX_DATA,	///< The size of the output buffer is not large enough for the indices; call getSizeofDispatchDrawInputData() to determine the size required.
			kDispatchDrawErrorOutOfSpaceForBlockOffset =	SCE_GNMX_ERROR_DISPATCH_DRAW_OUT_OF_SPACE_FOR_BLOCK_OFFSET,	///< The block offset table is not large enough; call getSizeofDispatchDrawInputData() to determine the number of blocks required.
			kDispatchDrawErrorUnrepresentableOffset =		SCE_GNMX_ERROR_DISPATCH_DRAW_UNREPRESENTABLE_OFFSET, ///< Description to be specified.
		} DispatchDrawStatus;

		/** @brief Calculates an upper bound on the maximum data size and number of blocks required for an index buffer of size <c><i>numIndicesIn</i></c> (which also has a <c><i>primType</i></c> of #Gnm::kPrimitiveTypeTriList or #Gnm::kPrimitiveTypeTriStrip format).
		 *
		 * @param[out]	pDispatchDrawTriangleCullIndexData	Receives the information about the index block data that will be generated by createDispatchDrawInputData(). Only the header and <c>m_numIndexDataBlocks</c> are filled out.
		 * @param[in]	primType							The primitive type of <c><i>pIndicesIn</i></c>, which can be either #Gnm::kPrimitiveTypeTriList or #Gnm::kPrimitiveTypeTriStrip.
		 * @param[in]	numIndicesIn						The number of indices in the 16-bit index list, which should be a multiple of three for #Gnm::kPrimitiveTypeTriList.
		 *
		 * @return The size required on success or a #DispatchDrawStatus value (<0) in the event of an error.
		 */
		SCE_GNMX_EXPORT int64_t getMaxSizeofDispatchDrawInputData(DispatchDrawTriangleCullIndexData *pDispatchDrawTriangleCullIndexData, Gnm::PrimitiveType primType, uint32_t numIndicesIn);

		/** @brief Calculates the required size of input data buffer and number of blocks for DispatchDrawTriangleCullData constructed from input indices in #Gnm::kPrimitiveTypeTriList or #Gnm::kPrimitiveTypeTriStrip format.
		 *
		 * @param[out]	pDispatchDrawTriangleCullIndexData	Receives the information about the index block data that will be generated by createDispatchDrawInputData(). It is fully filled out except for <c>m_bufferInputIndexData</c>'s base address.
		 * @param[in]	primType							The primitive type of <c><i>pIndicesIn</i></c>, which can be either #Gnm::kPrimitiveTypeTriList or #Gnm::kPrimitiveTypeTriStrip.
		 * @param[in]	numIndicesIn						The number of indices in the 16-bit index list, which should be a multiple of three for #Gnm::kPrimitiveTypeTriList.
		 * @param[in]	pIndicesIn							The 16-bit input index list.
		 *
		 * @return The size required on success or a #DispatchDrawStatus value (<0) in the event of an error.
		 */
		SCE_GNMX_EXPORT int64_t getSizeofDispatchDrawInputData(DispatchDrawTriangleCullIndexData *pDispatchDrawTriangleCullIndexData, Gnm::PrimitiveType primType, uint32_t numIndicesIn, uint16_t const* pIndicesIn);

		/** @brief Constructs an input data buffer for DispatchDrawTriangleCullData from input indices in #Gnm::kPrimitiveTypeTriList or #Gnm::kPrimitiveTypeTriStrip format.
		 *
		 * The index data format constructed has the following internal structure:
		 * 
		 * @code
		 * uint32_t aIndexBlockOffset[m_numIndexDataBlocks];	//byte offsets from start of pDispatchDrawTriangleCullIndexData->m_bufferInputIndexData to each of pDispatchDrawTriangleCullIndexData->m_numIndexDataBlocks blocks.
		 * {   //block[0] @ pDispatchDrawTriangleCullIndexData->m_bufferInputIndexData.getBaseAddress() + aIndexBlockOffset[0]; the start of each block must be 4-byte aligned
		 *     uint16_t numVtx;	// count of unique vertices used by block[0]; each block may contain up to 256 unique vertices
		 *     uint16_t numTri;	// count of triangles in block[0]; each block may contain up to 512 triangles
		 *     uint16_t aVtxIndex[numVtx]; // up to 256 unique 16-bit indices used in this block
		 *     uint8_t  aTri[numTri*3];    // up to 512 triangles stored as triplets of 8-bit block local indices into aVtxIndex[]
		 *     uint8_t  aPad[(0-(numVtx*2 + numTri*3)) & 0x3];  // each block is padded to a 4-byte aligned boundary
		 * }
		 * //...
		 * {   //block[m_numIndexDataBlocks-1] @ pDispatchDrawTriangleCullIndexData->m_bufferInputIndexData.getBaseAddress() + aIndexBlockOffset[m_numIndexDataBlocks-1]
		 *     //...
		 * }
		 * @endcode
		 *
		 * As it is possible that the input triangles might be rendered in an order dependent way, createDispatchDrawInputData() does not reorder triangles.
		 * As such, it is generally beneficial to performance to optimize the triangle order for opaque/order independent geometry. This is done by applying the
		 * vertex cache optimizer, before calling createDispatchDrawInputData(), as this will improve locality of vertex reuse and improve the size and 
		 * efficiency of the generated index block data. 
		 *
		 * @param[in,out]	pDispatchDrawTriangleCullIndexData		<c>m_numIndexDataBlocks</c> is used to lay out the output data. Then this entire structure is filled out including setting <c>m_bufferInputIndexData</c>'s base address to <c><i>pIndexDataOut</i></c>.
		 * @param[in]		primType								The primitive type of <c><i>pIndicesIn</i></c>, either #Gnm::kPrimitiveTypeTriList or #Gnm::kPrimitiveTypeTriStrip.
		 * @param[in]		numIndicesIn							The number of indices in the 16-bit index list. Should be a multiple of three for #Gnm::kPrimitiveTypeTriList.
		 * @param[in]		pIndicesIn								The 16-bit input index list.
		 * @param[out]		pIndexDataOut							A pointer to an output buffer to write data to, which should have buffer (4-byte) alignment; getSizeofDispatchDrawInputData() should be called to determine the size required.
		 * @param[in]		sizeofIndexDataOut						The size of the output buffer <c><i>pIndexDataOut</i></c>. Call getSizeofDispatchDrawInputData() to determine the size required.
		 *
		 * @return The actual size of data written to <c><i>pIndexDataOut</i></c> or a #DispatchDrawStatus value (<0) in the event of an error.
		 */
		SCE_GNMX_EXPORT int64_t createDispatchDrawInputData(DispatchDrawTriangleCullIndexData *pDispatchDrawTriangleCullIndexData, Gnm::PrimitiveType primType, uint32_t const numIndicesIn, uint16_t const*const pIndicesIn, void*const pIndexDataOut, size_t const sizeofIndexDataOut);
	}
}

#endif /* _SCE_GNMX_DISPATCHDRAW_H */

