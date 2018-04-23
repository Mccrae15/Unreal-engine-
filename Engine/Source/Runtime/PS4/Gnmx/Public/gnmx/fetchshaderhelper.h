/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef _SCE_GNMX_FETCHSHADERHELPER_H
#define _SCE_GNMX_FETCHSHADERHELPER_H

#include <gnmx/shaderbinary.h>

namespace sce
{
	namespace Gnmx
	{
		/** @brief Computes the size of the fetch shader in bytes for a VS-stage vertex shader.
		 *
		 * @param[in] vsb The vertex shader to generate a fetch shader for.
		 *
		 * @return The size of the fetch shader in bytes.
		 *
		 * @see generateVsFetchShader()
		 */
		SCE_GNMX_EXPORT uint32_t computeVsFetchShaderSize(const VsShader *vsb);

		/** @brief Generates the fetch shader for a VS-stage vertex shader.
		 *
		 * This <b>Direct Mapping</b> variant of the function assumes that all vertex buffer slots map directly to the corresponding vertex shader input slots.
		 *
		 * @param[out] fs				Buffer to hold the fetch shader that this method generates. This buffer must be at least as large as the size that computeVsFetchShaderSize() returns.
		 * @param[out] shaderModifier	Receives a value that you need to pass to either of GfxContext::setVsShader() or VsShader::applyFetchShaderModifier().
		 * @param[in] vsb				A pointer to the vertex shader binary data.
		 *
		 * @see computeVsFetchShaderSize()
		 */
		SCE_GNMX_EXPORT void generateVsFetchShader(void *fs,
												   uint32_t* shaderModifier,
												   const VsShader *vsb);

		/** @brief Generates the fetch shader for a VS-stage vertex shader with instanced fetching.
		 *
		 * This <b>Direct Mapping</b> variant of the function assumes that all vertex buffer slots map directly to the corresponding vertex shader input slots.
		 *
		 * @note If <c><i>instancingData</i></c> is not <c>NULL</c>, it specifies the index (vertex index or instance id) to use to fetch each semantic input to the vertex shader.
		 *       Non-system-semantic vertex inputs are assigned sequential VertexInputSemantic::m_semantic values starting from <c>0</c> in the order declared in the PSSL source, and 
		 *       incremented for each vector, matrix row, or array element, regardless of which elements are used by the shader. Thus, for shaders that do not
		 *       use every declared vertex input, the largest <c>m_semantic</c> value plus 1 may be larger than <c><i>m_numInputSemantics</i></c>.
		 *
		 * @param[out] fs				Buffer to hold the fetch shader that this method generates. This buffer must be at least as large as the size that computeVsFetchShaderSize() returns.
		 * @param[out] shaderModifier	Receives a value that you need to pass to either of GfxContext::setVsShader() or VsShader::applyFetchShaderModifier().
		 * @param[in] vsb				A pointer to the vertex shader binary data.
		 * @param[in] instancingData				A pointer to a table describing the index to use for fetching shader entry data by VertexInputSemantic::m_semantic index. If read at or beyond <c><i>numElementsInInstancingData</i></c>, or if <c>NULL</c>, defaults to Vertex Index. 
		 * @param[in] numElementsInInstancingData	The size of the <c><i>instancingData</i></c> table, if not <c>NULL</c>. Generally, this value should match the number of non-system vector vertex inputs in the logical inputs to the vertex shader.
		 *
		 * @see computeVsFetchShaderSize()
		 */
		SCE_GNMX_EXPORT void generateVsFetchShader(void *fs,
												   uint32_t* shaderModifier,
												   const VsShader *vsb,
												   const Gnm::FetchShaderInstancingMode *instancingData, const uint32_t numElementsInInstancingData);

		/** @deprecated This function now requires the element count of the <c>instancingData</c> table.
		 * @brief Generates the fetch shader for a VS-stage vertex shader.
		 *
		 * This <b>Direct Mapping</b> variant of the function assumes that all vertex buffer slots map directly to the corresponding vertex shader input slots.
		 *
		 * @param[out] fs				Buffer to hold the fetch shader that this method generates. This buffer must be at least as large as the size that computeVsFetchShaderSize() returns.
		 * @param[out] shaderModifier	Receives a value that you need to pass to either of GfxContext::setVsShader() or VsShader::applyFetchShaderModifier().
		 * @param[in] vsb				A pointer to the vertex shader binary data.
		 * @param[in] instancingData	A pointer to a table describing the index to use to fetch the data for each shader entry. To default always to Vertex Index, pass <c>NULL</c> as this value.
		 *
		 * @see computeVsFetchShaderSize()
		 */
		SCE_GNM_API_DEPRECATED_MSG("please ensure instancingData is indexed by VertexInputSemantic::m_semantic and supply numElementsInInstancingData")
		void generateVsFetchShader(void *fs, uint32_t* shaderModifier, const VsShader *vsb, const Gnm::FetchShaderInstancingMode *instancingData)
		{
			generateVsFetchShader(fs, shaderModifier, vsb, instancingData, instancingData != nullptr ? 256 : 0);
		}
		
		/** @brief Generates the fetch shader for a VS-stage vertex shader with instanced fetching, while allowing for arbitrary remapping of vertex buffer slots to vertex shader input slots.
		 *
		 * This <b>Remapping Table</b> variant of the function uses the specified semantic remapping table to allow the arbitrary remapping of vertex buffer slots to vertex shader input slots.
		 *
		 * @param[out] fs             		Buffer to hold the fetch shader that this method generates. This buffer must be at least as large as the size that computeVsFetchShaderSize() returns.
		 * @param[out] shaderModifier   	Receives a value that you need to pass to either of GfxContext::setVsShader() or VsShader::applyFetchShaderModifier().
		 * @param[in] vsb                	A pointer to the vertex shader binary data.
		 * @param[in] semanticRemapTable	A pointer to the semantic remapping table that matches the vertex shader input with the Vertex Buffer definition. This table contains
		 * 									one element for each vertex buffer slot that may be bound. Each vertex buffer slot's table entry holds the index of the
		 * 									vertex shader input to associate with that vertex buffer (or <c>-1</c> if the buffer is unused.)
		 * @param[in] numElementsInRemapTable	The size of the table passed to <c><i>semanticRemapTable</i></c>.
		 *
		 * @see computeVsFetchShaderSize()
		 */
		SCE_GNMX_EXPORT void generateVsFetchShader(void *fs,
												   uint32_t* shaderModifier,
												   const VsShader *vsb,
												   const void *semanticRemapTable, const uint32_t numElementsInRemapTable);

		/** @brief Generates the fetch shader for a VS-stage vertex shader while allowing for arbitrary remapping of vertex buffer slots to vertex shader input slots.
		 *
		 * This <b>Remapping Table</b> variant of the function uses the specified semantic remapping table to allow the arbitrary remapping of vertex buffer slots to vertex shader input slots.
		 *
		 * @note Semantic remapping is not applied to <c><i>instancingData</i></c> table lookups, and it does not affect the required ordering or size of that table.
		 * @note If <c><i>instancingData</i></c> is not <c>NULL</c>, it specifies the index (vertex index or instance id) to use to fetch each semantic input to the vertex shader.
		 *       Non-system-semantic vertex inputs are assigned sequential VertexInputSemantic::m_semantic values starting from <c>0</c> in the order declared in the PSSL source, and
		 *       incremented for each vector, matrix row, or array element, regardless of which elements are used by the shader. Thus, for shaders that do not
		 *       use every declared vertex input, the largest <c>m_semantic</c> value plus 1 may be larger than <c><i>m_numInputSemantics</i></c>.
		 *
		 * @param[out] fs             			Buffer to hold the fetch shader that this method generates. This buffer must be at least as large as the size that computeVsFetchShaderSize() returns.
		 * @param[out] shaderModifier   		Receives a value that you need to pass to either of GfxContext::setVsShader() or VsShader::applyFetchShaderModifier().
		 * @param[in] vsb                		A pointer to the vertex shader binary data.
		 * @param[in] instancingData				A pointer to a table describing the index to use for fetching shader entry data by VertexInputSemantic::m_semantic index. If read at or beyond <c><i>numElementsInInstancingData</i></c>, or if <c>NULL</c>, defaults to Vertex Index. 
		 * @param[in] numElementsInInstancingData	The size of the <c><i>instancingData</i></c> table, if not <c>NULL</c>. Generally, this value should match the number of non-system vector vertex inputs in the logical inputs to the vertex shader.
		 * @param[in] semanticRemapTable		A pointer to the semantic remapping table that matches the vertex shader input with the Vertex Buffer definition. This table contains
		 * 										one element for each vertex buffer slot that may be bound. Each vertex buffer slot's table entry holds the index of the 
		 * 										vertex shader input to associate with that vertex buffer (or <c>-1</c> if the buffer is unused.)
		 * @param[in] numElementsInRemapTable	The size of the table passed to <c><i>semanticRemapTable</i></c>.
		 *
		 * @see computeVsFetchShaderSize()
		 */
		SCE_GNMX_EXPORT void generateVsFetchShader(void *fs,
												   uint32_t *shaderModifier,
												   const VsShader *vsb,
												   const Gnm::FetchShaderInstancingMode *instancingData, const uint32_t numElementsInInstancingData,
												   const void *semanticRemapTable, const uint32_t numElementsInRemapTable);

		/** @deprecated This function now requires the element count of the <c>instancingData</c> table.
		 * @brief Generates the fetch shader for a VS-stage vertex shader while allowing for arbitrary remapping of vertex buffer slots to vertex shader input slots.
		 *
		 * This <b>Remapping Table</b> variant of the function uses the specified semantic remapping table to allow for the arbitrary remapping of vertex buffer slots to vertex shader input slots.
		 *
		 * @param[out] fs             			Buffer to hold the fetch shader that this method generates. This buffer must be at least as large as the size that computeVsFetchShaderSize() returns.
		 * @param[out] shaderModifier   		Receives a value that you need to pass to either of GfxContext::setVsShader() or VsShader::applyFetchShaderModifier().
		 * @param[in] vsb                		A pointer to the vertex shader binary data.
		 * @param[in] instancingData			A pointer to a table describing the index to use to fetch the data for each shader entry. To default always to Vertex Index, pass <c>NULL</c> as this value.
		 * @param[in] semanticRemapTable		A pointer to the semantic remapping table that matches the vertex shader input with the Vertex Buffer definition. This table contains
		 * 										one element for each vertex buffer slot that may be bound. Each vertex buffer slot's table entry holds the index of the
		 * 										vertex shader input to associate with that vertex buffer (or <c>-1</c> if the buffer is unused.)
		 * @param[in] numElementsInRemapTable	The size of the table passed to <c><i>semanticRemapTable</i></c>.
		 */
		SCE_GNM_API_DEPRECATED_MSG("please ensure instancingData is indexed by VertexInputSemantic::m_semantic and supply numElementsInInstancingData")
		void generateVsFetchShader(void *fs, uint32_t* shaderModifier, const VsShader *vsb, const Gnm::FetchShaderInstancingMode *instancingData, const void *semanticRemapTable, const uint32_t numElementsInRemapTable)
		{
			generateVsFetchShader(fs, shaderModifier, vsb, instancingData, instancingData != nullptr ? 256 : 0, semanticRemapTable, numElementsInRemapTable);
		}

		/** @brief Computes the size of the fetch shader in bytes for an LS-stage vertex shader.
		 *
		 * @param[in] lsb		The vertex shader to generate a fetch shader for.
		 *
		 * @return 			The size of the fetch shader in bytes.
		 *
		 * @see generateLsFetchShader()
		 */
		SCE_GNMX_EXPORT uint32_t computeLsFetchShaderSize(const LsShader *lsb);

		/** @brief Generates the Fetch Shader for an LS-stage vertex shader.
		 *
		 * The <b>Direct Mapping</b> variant assumes that all vertex buffer slots map directly to the corresponding vertex shader input slots.
		 *	
		 * @param[out] fs				Buffer to hold the fetch shader that this method generates. This buffer must be at least as large as the size that computeLsFetchShaderSize() returns.
		 * @param[out] shaderModifier	Output value which will need to be passed to the GfxContext::setLsShader() function.
		 * @param[in] lsb				Pointer to LDS shader binary data.
		 *
		 * @see computeLsFetchShaderSize()
		 */
		SCE_GNMX_EXPORT void generateLsFetchShader(void *fs,
												   uint32_t *shaderModifier,
												   const LsShader *lsb);

		/** @brief Generates the Fetch Shader for an LS-stage vertex shader with instanced fetching.
		 *
		 * This <b>Direct Mapping</b> variant assumes that all vertex buffer slots map directly to the corresponding vertex shader input slots.
		 *
		 * @note If <c><i>instancingData</i></c> is not <c>NULL</c>, it specifies the index (vertex index or instance id) to use to fetch each semantic input to the vertex shader.
		 *       Non-system-semantic vertex inputs are assigned sequential VertexInputSemantic::m_semantic values starting from <c>0</c> in the order declared in the PSSL source, and
		 *       incremented for each vector, matrix row, or array element, regardless of which elements are used by the shader. Thus, for shaders that do not
		 *       use every declared vertex input, the largest <c>m_semantic</c> value plus 1 may be larger than <c><i>m_numInputSemantics</i></c>.
		 *
		 * @param fs				Buffer to hold the fetch shader that this method generates. This buffer must be at least as large as the size that computeLsFetchShaderSize() returns.
		 * @param shaderModifier	Receives a value which will need to be passed to GfxContext::setLsShader().
		 * @param lsb				A pointer to LDS shader binary data.
		 * @param[in] instancingData				A pointer to a table describing the index to use for fetching shader entry data by VertexInputSemantic::m_semantic index. If read at or beyond <c><i>numElementsInInstancingData</i></c>, or if <c>NULL</c>, defaults to Vertex Index.
		 * @param[in] numElementsInInstancingData	The size of the <c><i>instancingData</i></c> table, if not <c>NULL</c>. Generally, this value should match the number of non-system vector vertex inputs in the logical inputs to the vertex shader.
		 *
		 * @see computeLsFetchShaderSize()
		 */
		SCE_GNMX_EXPORT void generateLsFetchShader(void *fs,
												   uint32_t *shaderModifier,
												   const LsShader *lsb,
												   const Gnm::FetchShaderInstancingMode *instancingData, const uint32_t numElementsInInstancingData);

		/** @deprecated This function now requires the element count of the <c>instancingData</c> table.
		 * @brief Generates the Fetch Shader for an LS-stage vertex shader.
		 *
		 * This <b>Direct Mapping</b> variant assumes that all vertex buffer slots map directly to the corresponding vertex shader input slots.
		 *
		 * @param fs				Buffer to hold the fetch shader that this method generates. This buffer must be at least as large as the size that computeLsFetchShaderSize() returns.
		 * @param shaderModifier	Receives a value that you need to pass to GfxContext::setLsShader().
		 * @param lsb				A pointer to LDS shader binary data.
		 * @param instancingData	A pointer to a table describing the index to use to fetch the data for each shader entry. To default always to Vertex Index, pass <c>NULL</c> as this value.
		 *
		 * @see computeLsFetchShaderSize()
		 */
		SCE_GNM_API_DEPRECATED_MSG("please ensure instancingData is indexed by VertexInputSemantic::m_semantic and supply numElementsInInstancingData")
		void generateLsFetchShader(void *fs, uint32_t* shaderModifier, const LsShader *lsb, const Gnm::FetchShaderInstancingMode *instancingData)
		{
			generateLsFetchShader(fs, shaderModifier, lsb, instancingData, instancingData != nullptr ? 256 : 0);
		}

		/** @brief Generates the Fetch Shader for an LS-stage vertex shader while allowing for arbitrary remapping of vertex buffer slots to vertex shader input slots.
		 *
		 * This <b>Remapping Table</b> variant of the function uses the specified semantic remapping table to allow the arbitrary remapping of vertex buffer slots to vertex shader input slots.
		 *
		 * @note Semantic remapping is not applied to <c><i>instancingData</i></c> table lookups, and it does not affect the required ordering or size of that table.
		 * @note If <c><i>instancingData</i></c> is not <c>NULL</c>, it specifies the index (vertex index or instance id) to use to fetch each semantic input to the vertex shader.
		 *       Non-system-semantic vertex inputs are assigned sequential VertexInputSemantic::m_semantic values starting from <c>0</c> in the order declared in the PSSL source, and
		 *       incremented for each vector, matrix row, or array element, regardless of which elements are used by the shader. Thus, for shaders that do not
		 *       use every declared vertex input, the largest <c>m_semantic</c> value plus 1 may be larger than <c><i>m_numInputSemantics</i></c>.
		 *
		 * @param fs             			Buffer to hold the fetch shader that this method generates. This buffer must be at least as large as the size that computeLsFetchShaderSize() returns.
		 * @param shaderModifier   			Receives a value that you need to pass to either of GfxContext::setLsShader() or LsShader::applyFetchShaderModifier().
		 * @param lsb                		A pointer to LDS shader binary data.
		 * @param semanticRemapTable		A pointer to the semantic remapping table that matches the vertex shader input with the Vertex Buffer definition. This table contains
		 * 									one element for each vertex buffer slot that may be bound. Each vertex buffer slot's table entry holds the index of the
		 * 									vertex shader input to associate with the vertex buffer (or <c>0xFFFFFFFF</c> if the buffer is unused).
		 * @param numElementsInRemapTable	The size of the table passed to <c><i>semanticRemapTable</i></c>.
		 */
		SCE_GNMX_EXPORT void generateLsFetchShader(void *fs,
												   uint32_t *shaderModifier,
												   const LsShader *lsb,
												   const void *semanticRemapTable, const uint32_t numElementsInRemapTable);

		/** @brief Generates the Fetch Shader for an LS-stage vertex shader with instanced fetching, while allowing for arbitrary remapping of vertex buffer slots to vertex shader input slots.
		 *
		 * This <b>Remapping Table</b> variant of the function uses the specified semantic remapping table to allow the arbitrary remapping of vertex buffer slots to vertex shader input slots.
		 *
		 * @note Semantic remapping is not applied to <c><i>instancingData</i></c> table lookups, and does not affect the required ordering or size of that table.
		 * @note If <c><i>instancingData</i></c> is not <c>NULL</c>, it specifies the index (vertex index or instance id) to use to fetch each semantic input to the vertex shader.
		 *       Non-system-semantic vertex inputs are assigned sequential VertexInputSemantic::m_semantic values starting from <c>0</c> in the order declared in the PSSL source, and
		 *       incremented for each vector, matrix row, or array element, regardless of which elements are used by the shader. Thus, for shaders that do not
		 *       use every declared vertex input, the largest <c>m_semantic</c> value plus 1 may be larger than <c><i>m_numInputSemantics</i></c>.
		 *
		 * @param[out] fs						Buffer to hold the fetch shader that this method generates. This buffer must be at least as large as the size that computeLsFetchShaderSize() returns.
		 * @param[out] shaderModifier			Output value that you need to pass to either of GfxContext::setLsShader() function or LsShader::applyFetchShaderModifier().
		 * @param[in] lsb						Pointer to LDS shader binary data.
		 * @param[in] instancingData			A pointer to a table describing the index to use for fetching shader entry data by <c>VertexInputSemantic::m_semantic</c> index. If read at or beyond <c><i>numElementsInInstancingData</i></c>, or if <c>NULL</c>, defaults to Vertex Index. 
		 * @param[in] numElementsInInstancingData	The size of the <c><i>instancingData</i></c> table, if not <c>NULL</c>. Generally, this value should match the number of non-system vector vertex inputs in the logical inputs to the vertex shader.
		 * @param[in] semanticRemapTable		A pointer to the semantic remapping table that matches the vertex shader input with the Vertex Buffer definition. This table contains
		 * 										one element for each vertex buffer slot that may be bound. Each vertex buffer slot's table entry holds the index of the
		 * 										vertex shader input to associate with that vertex buffer (or <c>-1</c> if the buffer is unused.)
		 * @param[in] numElementsInRemapTable	Size of the <c><i>semanticRemapTable</i></c>.
		 */
		SCE_GNMX_EXPORT void generateLsFetchShader(void *fs,
												   uint32_t *shaderModifier,
												   const LsShader *lsb,
												   const Gnm::FetchShaderInstancingMode *instancingData, const uint32_t numElementsInInstancingData,
												   const void *semanticRemapTable, const uint32_t numElementsInRemapTable);

		/** @deprecated This function now requires the element count of the <c>instancingData</c> table.
		 * @brief Generates the Fetch Shader for an LS-stage vertex shader while allowing for arbitrary remapping of vertex buffer slots to vertex shader input slots.
		 *
		 * This <b>Remapping Table</b> variant of the function uses the specified semantic remapping table to allow for the arbitrary remapping of vertex buffer slots to vertex shader input slots.
		 *
		 * @param[out] fs						Buffer to hold the fetch shader that this method generates. This buffer must be at least as large as the size that computeLsFetchShaderSize() returns.
		 * @param[out] shaderModifier			Receives the value that you need to pass to either of GfxContext::setLsShader() or LsShader::applyFetchShaderModifier().
		 * @param[in] lsb						Pointer to LDS shader binary data.
		 * @param[in] instancingData			A pointer to a table describing the index to use to fetch the data for each shader entry. To default always to Vertex Index, pass <c>NULL</c> as this value.
		 * @param[in] semanticRemapTable		A pointer to the semantic remapping table that matches the vertex shader input with the Vertex Buffer definition. This table contains
		 * 										one element for each vertex buffer slot that may be bound. Each vertex buffer slot's table entry holds the index of the
		 * 										vertex shader input to associate with that vertex buffer (or <c>-1</c> if the buffer is unused.)
		 * @param[in] numElementsInRemapTable	Size of the <c><i>semanticRemapTable</i></c>.
		 */
		SCE_GNM_API_DEPRECATED_MSG("please ensure instancingData is indexed by VertexInputSemantic::m_semantic and supply numElementsInInstancingData")
		void generateLsFetchShader(void *fs, uint32_t* shaderModifier, const LsShader *lsb, const Gnm::FetchShaderInstancingMode *instancingData, const void *semanticRemapTable, const uint32_t numElementsInRemapTable)
		{
			generateLsFetchShader(fs, shaderModifier, lsb, instancingData, instancingData != nullptr ? 256 : 0, semanticRemapTable, numElementsInRemapTable);
		}

		/** @brief Computes the size of the fetch shader in bytes for an ES-stage vertex shader.
		 *
		 * @param[in] esb The vertex shader to generate a fetch shader for.
		 *
		 * @return The size of the fetch shader in bytes.
		 *
		 * @see generateEsFetchShader()
		 */
		SCE_GNMX_EXPORT uint32_t computeEsFetchShaderSize(const EsShader *esb);

		/** @brief Generates the fetch shader for an ES-stage vertex shader.
		 *
		 * The <b>Direct Mapping</b> variant assumes that all vertex buffer slots map directly to the corresponding vertex shader input slots.
		 *
		 * @param[out] fs					Buffer to hold the fetch shader that this method generates. This buffer must be at least as large as the size that computeEsFetchShaderSize() returns.
		 * @param[out] shaderModifier		Receives the value that you need to pass to either of GfxContext::setEsShader() or EsShader::applyFetchShaderModifier().
		 * @param[in] esb					Pointer to the export shader binary data.
		 *
		 * @see computeEsFetchShaderSize()
		 */
		SCE_GNMX_EXPORT void generateEsFetchShader(void *fs,
												   uint32_t *shaderModifier,
												   const EsShader *esb);

		/** @brief Generates the fetch shader for an ES-stage vertex shader with instanced fetching.
		 *
		 * This <b>Direct Mapping</b> variant of the function assumes that all vertex buffer slots map directly to the corresponding vertex shader input slots.
		 *
		 * @note If <c><i>instancingData</i></c> is not <c>NULL</c>, it specifies the index (vertex index or instance id) to use to fetch each semantic input to the vertex shader.
		 *       Non-system-semantic vertex inputs are assigned sequential VertexInputSemantic::m_semantic values starting from <c>0</c> in the order declared in the PSSL source, and
		 *       incremented for each vector, matrix row, or array element, regardless of which elements are used by the shader. Thus, for shaders that do not
		 *       use every declared vertex input, the largest <c>m_semantic</c> value plus 1 may be larger than <c><i>m_numInputSemantics</i></c>.
		 *
		 * @param[out] fs					Buffer to hold the fetch shader that this method generates. This buffer must be at least as large as the size that computeEsFetchShaderSize() returns.
		 * @param[out] shaderModifier		Receives the value that you need to pass to either of GfxContext::setEsShader() or EsShader::applyFetchShaderModifier().
		 * @param[in] esb					Pointer to the export shader binary data.
		 * @param[in] instancingData				A pointer to a table describing the index to use for fetching shader entry data by VertexInputSemantic::m_semantic index. If read at or beyond <c><i>numElementsInInstancingData</i></c>, or if <c>NULL</c>, defaults to Vertex Index.
		 * @param[in] numElementsInInstancingData	The size of the <c><i>instancingData</i></c> table, if not <c>NULL</c>. Generally, this value should match the number of non-system vector vertex inputs in the logical inputs to the vertex shader.
		 *
		 * @see computeEsFetchShaderSize()
		 */
		SCE_GNMX_EXPORT void generateEsFetchShader(void *fs,
												   uint32_t *shaderModifier,
												   const EsShader *esb,
												   const Gnm::FetchShaderInstancingMode *instancingData, const uint32_t numElementsInInstancingData);

		/** @deprecated This function now requires the element count of the <c>instancingData</c> table.
		 * @brief Generates the fetch shader for an ES-stage vertex shader.
		 *
		 * This <b>Direct Mapping</b> variant assumes that all vertex buffer slots map directly to the corresponding vertex shader input slots.
		 *
		 * @param[out] fs					Buffer to hold the fetch shader that this method generates. This buffer must be at least as large as the size that computeLsFetchShaderSize() returns.
		 * @param[out] shaderModifier		Receives the value that you need to pass to either of GfxContext::setEsShader() or EsShader::applyFetchShaderModifier().
		 * @param[in] esb					Pointer to the export shader binary data.
		 * @param[in] instancingData		Pointer to a table describing the index to use to fetch the data for each shader entry. To default always to Vertex Index, pass <c>NULL</c> as this value.
		 *
		 * @see computeEsFetchShaderSize()
		 */
		SCE_GNM_API_DEPRECATED_MSG("please ensure instancingData is indexed by VertexInputSemantic::m_semantic and supply numElementsInInstancingData")
		void generateEsFetchShader(void *fs, uint32_t* shaderModifier, const EsShader *esb, const Gnm::FetchShaderInstancingMode *instancingData)
		{
			generateEsFetchShader(fs, shaderModifier, esb, instancingData, instancingData != nullptr ? 256 : 0);
		}

		/** @brief Generates the Fetch Shader for a ES-stage vertex shader while allowing for arbitrary remapping of vertex buffer slots to vertex shader input slots.
		 *
		 * This <b>Remapping Table</b> variant of the function uses the specified semantic remapping table to allow the arbitrary remapping of vertex buffer slots to vertex shader input slots.
		 *
		 * @param[out] fs						Buffer to hold the fetch shader that this method generates. This buffer must be at least as large as the size that computeEsFetchShaderSize() returns.
		 * @param[out] shaderModifier			Receives the value that you need to pass to either of GfxContext::setEsShader() or EsShader::applyFetchShaderModifier().
		 * @param[in] esb						Pointer to the export shader binary data.
		 * @param[in] semanticRemapTable		A pointer to the semantic remapping table that matches the vertex shader input with the Vertex Buffer definition. This table contains one element for each vertex buffer slot
		 *									that may be bound. Each vertex buffer slot's table entry holds the index of the vertex shader input to associate with that vertex buffer (or <c>-1</c> if the buffer is unused.)
		 * @param[in] numElementsInRemapTable Size of the <c><i>semanticRemapTable</i></c>.
		 */
		SCE_GNMX_EXPORT void generateEsFetchShader(void *fs,
												   uint32_t *shaderModifier,
												   const EsShader *esb,
												   const void *semanticRemapTable, const uint32_t numElementsInRemapTable);

		/** @brief Generates the Fetch Shader for a ES-stage vertex shader with instanced fetching, while allowing for arbitrary remapping of vertex buffer slots to vertex shader input slots.
		 *
		 * This <b>Remapping Table</b> variant of the function uses the specified semantic remapping table to allow the arbitrary remapping of vertex buffer slots to vertex shader input slots.
		 *
		 * @note Semantic remapping is not applied to <c><i>instancingData</i></c> table lookups, and does not affect the required ordering or size of that table.
		 * @note If <c><i>instancingData</i></c> is not <c>NULL</c>, it specifies the index (vertex index or instance id) to use to fetch each semantic input to the vertex shader.
		 *       Non-system-semantic vertex inputs are assigned sequential VertexInputSemantic::m_semantic values starting from <c>0</c> in the order declared in the PSSL source, and
		 *       incremented for each vector, matrix row, or array element, regardless of which elements are used by the shader. Thus, for shaders that do not
		 *       use every declared vertex input, the largest <c>m_semantic</c> value plus 1 may be larger than <c><i>m_numInputSemantics</i></c>.
		 *
		 * @param[out] fs						Buffer to hold the fetch shader that this method generates. This buffer must be at least as large as the size that computeEsFetchShaderSize() returns.
		 * @param[out] shaderModifier			Receives the value that you need to pass to either of GfxContext::setEsShader() or EsShader::applyFetchShaderModifier().
		 * @param[in] esb						Pointer to the export shader binary data.
		 * @param[in] instancingData			A pointer to a table describing the index to use for fetching shader entry data by VertexInputSemantic::m_semantic index. If read at or beyond <c><i>numElementsInInstancingData</i></c>, or if <c>NULL</c>, defaults to Vertex Index.
		 * @param[in] numElementsInInstancingData	The size of the <c><i>instancingData</i></c> table, if not <c>NULL</c>. Generally, this value should match the number of non-system vector vertex inputs in the logical inputs to the vertex shader.
		 * @param[in] semanticRemapTable		A pointer to the semantic remapping table that matches the vertex shader input with the Vertex Buffer definition. This table contains one element for each vertex buffer slot
		 *									that may be bound. Each vertex buffer slot's table entry holds the index of the vertex shader input to associate with that vertex buffer (or <c>-1</c> if the buffer is unused.)
		 * @param[in] numElementsInRemapTable Size of the <c><i>semanticRemapTable</i></c>.
		 */
		SCE_GNMX_EXPORT void generateEsFetchShader(void *fs,
												   uint32_t *shaderModifier,
												   const EsShader *esb,
												   const Gnm::FetchShaderInstancingMode *instancingData, const uint32_t numElementsInInstancingData,
												   const void *semanticRemapTable, const uint32_t numElementsInRemapTable);

		/** @deprecated This function now requires the element count of the <c>instancingData</c> table.
		 * @brief Generates the Fetch Shader for a ES-stage compute shader while allowing for arbitrary remapping of vertex buffer slots to vertex shader input slots.
		 *
		 * This <b>Remapping Table</b> variant of the function uses the specified semantic remapping table to allow for the arbitrary remapping of vertex buffer slots to vertex shader input slots.
		 *
		 * @param[out] fs						Buffer to hold the fetch shader that this method generates. This buffer must be at least as large as the size that computeEsFetchShaderSize() returns.
		 * @param[out] shaderModifier			Receives the value that you need to pass to either of GfxContext::setEsShader() or EsShader::applyFetchShaderModifier().
		 * @param[in] esb						Pointer to the export shader binary data.
		 * @param[in] instancingData			Pointer to a table describing the index to use to fetch the data for each shader entry. To default always to Vertex Index, pass <c>NULL</c> as this value.
		 * @param[in] semanticRemapTable		A pointer to the semantic remapping table that matches the vertex shader input with the Vertex Buffer definition. This table contains one element for each vertex buffer slot
		 * 										that may be bound. Each vertex buffer slot's table entry holds the index of the vertex shader input to associate with that vertex buffer (or <c>-1</c> if the buffer is unused.)
		 * @param[in] numElementsInRemapTable Size of the <c><i>semanticRemapTable</i></c>.
		 */
		SCE_GNM_API_DEPRECATED_MSG("please ensure instancingData is indexed by VertexInputSemantic::m_semantic and supply numElementsInInstancingData")
		void generateEsFetchShader(void *fs, uint32_t* shaderModifier, const EsShader *esb, const Gnm::FetchShaderInstancingMode *instancingData, const void *semanticRemapTable, const uint32_t numElementsInRemapTable)
		{
			generateEsFetchShader(fs, shaderModifier, esb, instancingData, instancingData != nullptr ? 256 : 0, semanticRemapTable, numElementsInRemapTable);
		}

		/** @brief Computes the size of the fetch shader in bytes for the VS-stage of a CsVsShader.
		 *
		 * @param[in] csvsb 	The combined compute and vertex shader for which this function generates a VS-stage fetch shader.
		 * @return 			The size of the fetch shader in bytes.
		 *
		 * @see generateVsFetchShader()
		 */
		SCE_GNMX_EXPORT uint32_t computeVsFetchShaderSize(const CsVsShader *csvsb);

		/** @brief Generates the Fetch Shader for the VS-stage of a CsVsShader.
		 *
		 * This <b>DispatchDraw</b> variant of the function generates the fetch shader for the VS stage part of a DispatchDraw and Compute Vertex shader pair.
		 *
		 * @param[out] fs					Buffer to hold the fetch shader that this method generates. This buffer must be at least as large as the size that computeVsFetchShaderSize() returns.
		 * @param[out] shaderModifier		Receives the value that you need to pass to either of GfxContext::setCsVsShader() or CsVsShader::applyFetchShaderModifiers().
		 * @param[in] csvsb					The combined compute and vertex shader for which this function generates a VS-stage fetch shader.
		 *
		 * @see computeVsFetchShaderSize()
		 */
		SCE_GNMX_EXPORT void generateVsFetchShader(void *fs,
												   uint32_t *shaderModifier,
												   const CsVsShader *csvsb);

		/** @brief Generates the Fetch Shader for the VS-stage of a CsVsShader with instanced fetching.
		 *
		 * This <b>DispatchDraw</b> variant of the function generates the fetch shader for the VS-stage part of a DispatchDraw/Compute Vertex shader pair.
		 *
		 * @note If <c><i>instancingData</i></c> is not <c>NULL</c>, it specifies the index (vertex index or instance id) to use to fetch each semantic input to the vertex shader.
		 *       Non-system-semantic vertex inputs are assigned sequential VertexInputSemantic::m_semantic values starting from <c>0</c> in the order declared in the PSSL source, and
		 *       incremented for each vector, matrix row, or array element, regardless of which elements are used by the shader. Thus, for shaders that do not
		 *       use every declared vertex input, the largest <c>m_semantic</c> value plus 1 may be larger than <c><i>m_numInputSemantics</i></c>.
		 *			 
		 * @param[out] fs					Buffer to hold the fetch shader that this method generates. This buffer must be at least as large as the size that computeVsFetchShaderSize() returns.
		 * @param[out] shaderModifier		Receives the value that you need to pass to either of GfxContext::setCsVsShader() or CsVsShader::applyFetchShaderModifiers().
		 * @param[in] csvsb					The combined compute and vertex shader for which this function generates a VS-stage fetch shader.
		 * @param[in] instancingData		A pointer to a table describing the index to use for fetching shader entry data by VertexInputSemantic::m_semantic index. If read at or beyond <c><i>numElementsInInstancingData</i></c>, or if <c>NULL</c>, defaults to Vertex Index.
		 * @param[in] numElementsInInstancingData	The size of the <c><i>instancingData</i></c> table, if not <c>NULL</c>. Generally, this value should match the number of non-system vector vertex inputs in the logical inputs to the vertex shader.
		 *
		 * @see computeVsFetchShaderSize()
		 */
		SCE_GNMX_EXPORT void generateVsFetchShader(void *fs,
												   uint32_t *shaderModifier,
												   const CsVsShader *csvsb,
												   const Gnm::FetchShaderInstancingMode *instancingData, const uint32_t numElementsInInstancingData);

		/** @deprecated This function now requires the element count of the <c>instancingData</c> table.
		 * @brief Generates the Fetch Shader for a VS-stage compute shader.
		 *
		 * This <b>DispatchDraw</b> variant of the function generates the fetch shader for the VS stage part of a DispatchDraw and Compute Vertex shader pair.
		 *			 
		 * @param[out] fs					Buffer to hold the fetch shader that this method generates. This buffer must be at least as large as the size that computeVsFetchShaderSize() returns.
		 * @param[out] shaderModifier		Receives the value that you need to pass to either of GfxContext::setCsVsShader() or CsVsShader::applyFetchShaderModifiers().
		 * @param[in] csvsb					The combined compute and vertex shader for which this function generates a VS-stage fetch shader.
		 * @param[in] instancingData		Pointer to a table describing the index to use to fetch the data for each shader entry. To default always to Vertex Index, pass <c>NULL</c> as this value.
		 *
		 * @see computeCsFetchShaderSize()
		 */
		SCE_GNM_API_DEPRECATED_MSG("please ensure instancingData is indexed by VertexInputSemantic::m_semantic and supply numElementsInInstancingData")
		void generateVsFetchShader(void *fs, uint32_t* shaderModifier, const CsVsShader *csvsb, const Gnm::FetchShaderInstancingMode *instancingData)
		{
			generateVsFetchShader(fs, shaderModifier, csvsb, instancingData, instancingData != nullptr ? 256 : 0);
		}

		/** @brief Generates the Fetch Shader for the VS-stage of a CsVsShader, while allowing for arbitrary remapping of vertex buffer slots to vertex shader input slots.
		 *
		 * This <b>Remapping Table</b> variant of the function uses the specified semantic remapping table to allow the arbitrary remapping of vertex buffer slots to vertex shader input slots.
		 *
		 * @param[out] fs						Buffer to hold the fetch shader that this method generates for the VS stage part of a DispatchDraw and Compute Vertex shader pair. This buffer must be at least as large as the size that computeVsFetchShaderSize() returns.
		 * @param[out] shaderModifier			Receives the value that you need to pass to either of GfxContext::setCsVsShader() or CsVsShader::applyFetchShaderModifiers().
		 * @param[in] csvsb						The combined compute and vertex shader for which this function generates a VS-stage fetch shader.
		 * @param[in] semanticRemapTable		A pointer to the semantic remapping table that matches the vertex shader input with the Vertex Buffer definition. This table contains 
		 * 										one element for each vertex buffer slot that may be bound. Each vertex buffer slot's table entry holds the index of the
		 * 										vertex shader input to associate with that vertex buffer (or <c>-1</c> if the buffer is unused.)
		 * @param[in] numElementsInRemapTable Size of the <c><i>semanticRemapTable</i></c>.
		 *
		 * @see computeVsFetchShaderSize()
		 */
		SCE_GNMX_EXPORT void generateVsFetchShader(void *fs,
												   uint32_t *shaderModifier,
												   const CsVsShader *csvsb,
												   const void *semanticRemapTable, const uint32_t numElementsInRemapTable);

		/** @brief Generates the Fetch Shader for the VS-stage of a CsVsShader with instanced fetching, while allowing for arbitrary remapping of vertex buffer slots to vertex shader input slots.
		 *
		 * This <b>Remapping Table</b> variant of the function uses the specified semantic remapping table to allow the arbitrary remapping of vertex buffer slots to vertex shader input slots.
		 *
		 * @note Semantic remapping is not applied to <c><i>instancingData</i></c> table lookups, and does not affect the required ordering or size of that table.
		 * @note If <c><i>instancingData</i></c> is not <c>NULL</c>, it specifies the index (vertex index or instance id) to use to fetch each semantic input to the vertex shader.
		 *       Non-system-semantic vertex inputs are assigned sequential VertexInputSemantic::m_semantic values starting from <c>0</c> in the order declared in the PSSL source, and
		 *       incremented for each vector, matrix row, or array element, regardless of which elements are used by the shader. Thus, for shaders that do not
		 *       use every declared vertex input, the largest <c>m_semantic</c> value plus 1 may be larger than <c><i>m_numInputSemantics</i></c>.
		 *
		 * @param[out] fs						Buffer to hold the fetch shader that this method generates for the VS stage part of a DispatchDraw and Compute Vertex shader pair. This buffer must be at least as large as the size that computeVsFetchShaderSize() returns.
		 * @param[out] shaderModifier			Receives the value that you need to pass to either of GfxContext::setCsVsShader() or CsVsShader::applyFetchShaderModifiers().
		 * @param[in] csvsb						The combined compute and vertex shader for which this function generates a VS-stage fetch shader.
		 * @param[in] instancingData			A pointer to a table describing the index to use for fetching shader entry data by VertexInputSemantic::m_semantic index. If read at or beyond <c><i>numElementsInInstancingData</i></c>, or if <c>NULL</c>, defaults to Vertex Index.
		 * @param[in] numElementsInInstancingData	The size of the <c><i>instancingData</i></c> table, if not <c>NULL</c>. Generally, this value should match the number of non-system vector vertex inputs in the logical inputs to the vertex shader.
		 * @param[in] semanticRemapTable		A pointer to the semantic remapping table that matches the vertex shader input with the Vertex Buffer definition. This table contains 
		 * 										one element for each vertex buffer slot that may be bound. Each vertex buffer slot's table entry contains the index of the vertex 
		 * 										shader input that the vertex buffer should map to (or -1, if the buffer is unused).
		 * @param[in] numElementsInRemapTable Size of the <c><i>semanticRemapTable</i></c>.
		 *
		 * @see computeVsFetchShaderSize()
		 */
		SCE_GNMX_EXPORT void generateVsFetchShader(void *fs,
												   uint32_t *shaderModifier,
												   const CsVsShader *csvsb,
												   const Gnm::FetchShaderInstancingMode *instancingData, const uint32_t numElementsInInstancingData, 
												   const void *semanticRemapTable, const uint32_t numElementsInRemapTable);

		/** @deprecated This function now requires the element count of the <c>instancingData</c> table.
		 * @brief Generates the Fetch Shader for a VS-stage compute shader while allowing for arbitrary remapping of vertex buffer slots to vertex shader input slots.
		 *
		 * This <b>Remapping Table</b> variant of the function uses the specified semantic remapping table to allow for the arbitrary remapping of vertex buffer slots to vertex shader input slots.
		 *
		 * @param[out] fs						Buffer to hold the fetch shader that this method generates for the VS stage part of a DispatchDraw and Compute Vertex shader pair. This buffer must be at least as large as the size that computeVsFetchShaderSize() returns.
		 * @param[out] shaderModifier			Receives the value that you need to pass to either of GfxContext::setCsVsShader() or CsVsShader::applyFetchShaderModifiers().
		 * @param[in] csvsb						The combined compute and vertex shader for which this function generates a VS-stage fetch shader.
		 * @param[in] instancingData			Pointer to a table describing the index to use to fetch the data for each shader entry. To default always to Vertex Index, pass <c>NULL</c> as this value.
		 * @param[in] semanticRemapTable		A pointer to the semantic remapping table that matches the vertex shader input with the Vertex Buffer definition. This table contains 
		 * 										one element for each vertex buffer slot that may be bound. Each vertex buffer slot's table entry holds the index of the
		 * 										vertex shader input to associate with that vertex buffer (or <c>-1</c> if the buffer is unused.)
		 * @param[in] numElementsInRemapTable Size of the <c><i>semanticRemapTable</i></c>.
		 *
		 * @see computeCsFetchShaderSize()
		 */
		SCE_GNM_API_DEPRECATED_MSG("please ensure instancingData is indexed by VertexInputSemantic::m_semantic and supply numElementsInInstancingData")
		void generateVsFetchShader(void *fs, uint32_t* shaderModifier, const CsVsShader *csvsb, const Gnm::FetchShaderInstancingMode *instancingData, const void *semanticRemapTable, const uint32_t numElementsInRemapTable)
		{
			generateVsFetchShader(fs, shaderModifier, csvsb, instancingData, instancingData != nullptr ? 256 : 0, semanticRemapTable, numElementsInRemapTable);
		}

		/** @brief Computes the size of the fetch shader in bytes for the CS-stage of a CsVsShader.
		 *
		 * @param[in] csvsb 	The combined compute and vertex shader for which this method generates a CS-stage fetch shader.
		 *
		 * @return 			The size of the fetch shader in bytes.
		 *
		 * @see generateCsFetchShader()
		 */
		SCE_GNMX_EXPORT uint32_t computeCsFetchShaderSize(const CsVsShader *csvsb);

		/** @brief Generates the Fetch Shader for the CS stage of a CsVsShader.
		 *
		 * This <b>Direct Mapping</b> variant of the function assumes that all vertex buffer slots map directly to the corresponding vertex shader input slots.
		 *
		 * @param[out] fs             Buffer to hold the fetch shader that this method generates. This buffer must be at least as large as the size that computeCsFetchShaderSize() returns.
		 * @param[out] shaderModifier Pass this output value to one of either of the GfxContext::setCsVsShaders() or CsVsShader::applyFetchShaderModifiers() methods.
		 * @param[in] csvsb          The combined compute and vertex shader for which this method generates a CS-stage fetch shader.
		 *
		 * @see computeCsFetchShaderSize()
		 */
		SCE_GNMX_EXPORT void generateCsFetchShader(void *fs,
												   uint32_t *shaderModifier,
												   const CsVsShader *csvsb);

		/** @brief Generates the Fetch Shader for the CS stage of a CsVsShader with instanced fetching.
		 *
		 * This <b>Direct Mapping</b> variant of the function assumes that all vertex buffer slots map directly to the corresponding vertex shader input slots.
		 *
		 * @note If <c><i>instancingData</i></c> is not <c>NULL</c>, it specifies the index (vertex index or instance id) to use to fetch each semantic input to the vertex shader.
		 *       Non-system-semantic vertex inputs are assigned sequential VertexInputSemantic::m_semantic values starting from <c>0</c> in the order declared in the PSSL source, and
		 *       incremented for each vector, matrix row, or array element, regardless of which elements are used by the shader. Thus, for shaders that do not
		 *       use every declared vertex input, the largest <c>m_semantic</c> value plus 1 may be larger than <c><i>m_numInputSemantics</i></c>.
		 *
		 * @param[out] fs             Buffer into which this method generates the fetch shader. This buffer must be at least as large as the size that computeCsFetchShaderSize() returns.
		 * @param[out] shaderModifier Pass this output value to one of either of the GfxContext::setCsVsShaders() or CsVsShader::applyFetchShaderModifiers() methods.
		 * @param[in] csvsb          The combined compute and vertex shader for which to generate a CS-stage fetch shader.
		 * @param[in] instancingData				A pointer to a table describing the index to use for fetching shader entry data by VertexInputSemantic::m_semantic index. If read at or beyond <c><i>numElementsInInstancingData</i></c>, or if <c>NULL</c>, defaults to Vertex Index.
		 * @param[in] numElementsInInstancingData	The size of the <c><i>instancingData</i></c> table, if not <c>NULL</c>. Generally, this value should match the number of non-system vector vertex inputs in the logical inputs to the vertex shader.
		 *
		 * @see computeCsFetchShaderSize()
		 */
		SCE_GNMX_EXPORT void generateCsFetchShader(void *fs,
												   uint32_t *shaderModifier,
												   const CsVsShader *csvsb,
												   const Gnm::FetchShaderInstancingMode *instancingData, const uint32_t numElementsInInstancingData);

		/** @deprecated This function now requires the element count of the <c>instancingData</c> table.
		 * @brief Generates the Fetch Shader for a CS-stage compute shader (it is most often used for a compute shader embedded in a CsVsShader).
		 *
		 * This <b>Direct Mapping</b> variant of the function assumes that all vertex buffer slots map directly to the corresponding vertex shader input slots.
		 *
		 * @param[out] fs             Buffer to hold the fetch shader that this method generates. This buffer must be at least as large as the size that computeCsFetchShaderSize() returns.
		 * @param[out] shaderModifier Receives the value that you need to pass to either one of GfxContext::setCsVsShaders() or CsVsShader::applyFetchShaderModifiers().
		 * @param[in] csvsb          The combined compute and vertex shader to generate a CS-stage fetch shader for.
		 * @param[in] instancingData Pointer to a table describing the index to use to fetch the data for each shader entry. To default always to Vertex Index, pass <c>NULL</c> as this value.
		 * @see computeCsFetchShaderSize()
		 */
		SCE_GNM_API_DEPRECATED_MSG("please ensure instancingData is indexed by VertexInputSemantic::m_semantic and supply numElementsInInstancingData")
		void generateCsFetchShader(void *fs, uint32_t* shaderModifier, const CsVsShader *csvsb, const Gnm::FetchShaderInstancingMode *instancingData)
		{
			generateCsFetchShader(fs, shaderModifier, csvsb, instancingData, instancingData != nullptr ? 256 : 0);
		}

		/** @brief Generates the Fetch Shader for the CS-stage of a CsVsShader while allowing for arbitrary remapping of vertex buffer slots to vertex shader input slots.
		 *
		 * This <b>Remapping Table</b> variant of the function uses the specified semantic remapping table to allow the arbitrary remapping of vertex buffer slots to vertex shader input slots.
		 *
		 * @param[out] fs             Buffer to hold the fetch shader that this method generates. This buffer must be at least as large as the size that computeCsFetchShaderSize() returns.
		 * @param[out] shaderModifier Pass this output value to one of either of the GfxContext::setCsVsShaders() or CsVsShader::applyFetchShaderModifiers() methods.
		 * @param[in] csvsb          The combined compute and vertex shader for which this method generates a CS-stage fetch shader.
		 * @param[in] semanticRemapTable		A pointer to the semantic remapping table that matches the vertex shader input with the Vertex Buffer definition. This table contains 
		 * 										one element for each vertex buffer slot that may be bound. Each vertex buffer slot's table entry holds the index of the vertex 
		 * 										shader input to associate with that the vertex buffer (or <c>0xFFFFFFFF</c>, if the buffer is unused).
		 * @param[in] numElementsInRemapTable Size of the <c><i>semanticRemapTable</i></c>.
		 *
		 * @see computeCsFetchShaderSize()
		 */
		SCE_GNMX_EXPORT void generateCsFetchShader(void *fs,
												   uint32_t *shaderModifier,
												   const CsVsShader *csvsb,
												   const void *semanticRemapTable, const uint32_t numElementsInRemapTable);

		/** @brief Generates the Fetch Shader for the CS-stage of a CsVsShader with instanced fetching, while allowing for arbitrary remapping of vertex buffer slots to vertex shader input slots.
		 *
		 * This <b>Remapping Table</b> variant of the function uses the specified semantic remapping table to allow the arbitrary remapping of vertex buffer slots to vertex shader input slots.
		 *
		 * @note Semantic remapping is not applied to <c><i>instancingData</i></c> table lookups, and it does not affect the required ordering or size of that table.
		 * @note If <c><i>instancingData</i></c> is not <c>NULL</c>, it specifies the index (vertex index or instance id) to use to fetch each semantic input to the vertex shader.
		 *       Non-system-semantic vertex inputs are assigned sequential VertexInputSemantic::m_semantic values starting from <c>0</c> in the order declared in the PSSL source, and
		 *       incremented for each vector, matrix row, or array element, regardless of which elements are used by the shader. Thus, for shaders that do not
		 *       use every declared vertex input, the largest <c>m_semantic</c> value plus 1 may be larger than <c><i>m_numInputSemantics</i></c>.
		 *
		 * @param[out] fs             Buffer to hold the fetch shader that this method generates. This buffer must be at least as large as the size that computeCsFetchShaderSize() returns.
		 * @param[out] shaderModifier Pass this output value to one of either of the GfxContext::setCsVsShaders() or CsVsShader::applyFetchShaderModifiers() methods.
		 * @param[in] csvsb          The combined compute and vertex shader for which this method generates a CS-stage fetch shader.
		 * @param[in] instancingData				A pointer to a table describing the index to use for fetching shader entry data by VertexInputSemantic::m_semantic index. If read at or beyond <c><i>numElementsInInstancingData</i></c>, or if <c>NULL</c>, defaults to Vertex Index.
		 * @param[in] numElementsInInstancingData	The size of the <c><i>instancingData</i></c> table, if not <c>NULL</c>. Generally, this value should match the number of non-system vector vertex inputs in the logical inputs to the vertex shader.
		 * @param[in] semanticRemapTable		A pointer to the semantic remapping table that matches the vertex shader input with the Vertex Buffer definition. This table contains 
		 * 										one element for each vertex buffer slot that may be bound. Each vertex buffer slot's table entry holds the index of the vertex 
		 * 										shader input to associate with that vertex buffer (or <c>0xFFFFFFFF</c>, if the buffer is unused).
		 * @param[in] numElementsInRemapTable Size of the <c><i>semanticRemapTable</i></c>.
		 *
		 * @see computeCsFetchShaderSize()
		 */
		SCE_GNMX_EXPORT void generateCsFetchShader(void *fs,
												   uint32_t *shaderModifier,
												   const CsVsShader *csvsb,
												   const Gnm::FetchShaderInstancingMode *instancingData, const uint32_t numElementsInInstancingData, 
												   const void *semanticRemapTable, const uint32_t numElementsInRemapTable);

		/** @deprecated This function now requires the element count of the <c>instancingData</c> table.
		 * @brief Generates the Fetch Shader for a CS-stage compute shader while allowing for arbitrary remapping of vertex buffer slots to vertex shader input slots; it is most often used 
		 *			for a compute shader embedded in a CsVsShader.
		 *
		 * This <b>Remapping Table</b> variant of the function uses the specified semantic remapping table to allow for the arbitrary remapping of vertex buffer slots to vertex shader input slots.
		 *
		 * @param[out] fs             Buffer to hold the fetch shader that this method generates. This buffer must be at least as large as the size that computeCsFetchShaderSize() returns.
		 * @param[out] shaderModifier Receives the value that you need to pass to either of GfxContext::setCsVsShaders() or CsVsShader::applyFetchShaderModifiers().
		 * @param[in] csvsb          The combined compute and vertex shader for which this function generates a CS-stage fetch shader.
		 * @param[in] instancingData Pointer to a table describing the index to use to fetch the data for each shader entry. To default always to Vertex Index, pass <c>NULL</c> as this value.
		 * @param[in] semanticRemapTable	A pointer to the semantic remapping table that matches the vertex shader input with the Vertex Buffer definition. This table contains 
		 * 									one element for each vertex buffer slot that may be bound. Each vertex buffer slot's table entry holds the index of the vertex 
		 * 									shader input to associate with that vertex buffer (or <c>0xFFFFFFFF</c>, if the buffer is unused).
		 * @param[in] numElementsInRemapTable Size of the <c><i>semanticRemapTable</i></c>.
		 */
		SCE_GNM_API_DEPRECATED_MSG("please ensure instancingData is indexed by VertexInputSemantic::m_semantic and supply numElementsInInstancingData")
		void generateCsFetchShader(void *fs, uint32_t* shaderModifier, const CsVsShader *csvsb, const Gnm::FetchShaderInstancingMode *instancingData, const void *semanticRemapTable, const uint32_t numElementsInRemapTable)
		{
			generateCsFetchShader(fs, shaderModifier, csvsb, instancingData, instancingData != nullptr ? 256 : 0, semanticRemapTable, numElementsInRemapTable);
		}

		/** @brief Computes the size of the fetch shader in bytes for a CsShader, which will typically be the CS-stage shader embedded in a CsVsShader.
		 *
		 * @param[in] csb   The compute shader for which this method generates a CS-stage fetch shader. Must have CsShader::m_version <c> >= 1</c>.
		 *
		 * @return 			The size of the fetch shader in bytes.
		 *
		 * @see generateCsFetchShader()
		 */
		SCE_GNMX_EXPORT uint32_t computeCsFetchShaderSize(const CsShader *csb);

		/** @brief Generates the Fetch Shader for a CsShader, which will typically be the CS-stage shader embedded in a CsVsShader.
		 *
		 * This <b>Direct Mapping</b> variant of the function assumes that all vertex buffer slots map directly to the corresponding vertex shader input slots.
		 *
		 * @param[out] fs             Buffer to hold the fetch shader that this method generates. This buffer must be at least as large as the size that computeCsFetchShaderSize() returns.
		 * @param[out] shaderModifier Receives the value that you need to pass to either of GfxContext::setCsVsShaders() or CsVsShader::applyFetchShaderModifiers().
		 * @param[in] csb				The compute shader for which this method generates a CS-stage fetch shader. Must have CsShader::m_version <c> >= 1</c>.
		 *
		 * @see computeCsFetchShaderSize()
		 */
		SCE_GNMX_EXPORT void generateCsFetchShader(void *fs,
												   uint32_t *shaderModifier,
												   const CsShader *csb);

		/** @brief Generates the Fetch Shader for a CsShader with instanced fetching, which is typically the CS-stage shader embedded in a CsVsShader.
		 *
		 * This <b>Direct Mapping</b> variant of the function assumes that all vertex buffer slots map directly to the corresponding vertex shader input slots.
		 *
		 * @note If <c><i>instancingData</i></c> is not <c>NULL</c>, it specifies the index (vertex index or instance id) to use to fetch each semantic input to the vertex shader.
		 *       Non-system-semantic vertex inputs are assigned sequential VertexInputSemantic::m_semantic values starting from <c>0</c> in the order declared in the PSSL source, and
		 *       incremented for each vector, matrix row, or array element, regardless of which elements are used by the shader. Thus, for shaders that do not
		 *       use every declared vertex input, the largest <c>m_semantic</c> value plus 1 may be larger than <c><i>m_numInputSemantics</i></c>.
		 *
		 * @param[out] fs             Buffer to hold the fetch shader that this method generates. This buffer must be at least as large as the size that computeCsFetchShaderSize() returns.
		 * @param[out] shaderModifier Receives the value that you need to pass to either of GfxContext::setCsVsShaders() or CsVsShader::applyFetchShaderModifiers().
		 * @param[in] csb				The compute shader for which this method generates a CS-stage fetch shader. Must have CsShader::m_version <c> >= 1</c>.
		 * @param[in] instancingData				A pointer to a table describing the index to use for fetching shader entry data by VertexInputSemantic::m_semantic index. If read at or beyond <c><i>numElementsInInstancingData</i></c>, or if <c>NULL</c>, defaults to Vertex Index.
		 * @param[in] numElementsInInstancingData	The size of the <c><i>instancingData</i></c> table, if not <c>NULL</c>. Generally, this value should match the number of non-system vector vertex inputs in the logical inputs to the vertex shader.
		 *
		 * @see computeCsFetchShaderSize()
		 */
		SCE_GNMX_EXPORT void generateCsFetchShader(void *fs,
												   uint32_t *shaderModifier,
												   const CsShader *csb,
												   const Gnm::FetchShaderInstancingMode *instancingData, const uint32_t numElementsInInstancingData);

		/** @deprecated This function now requires the element count of the <c>instancingData</c> table.
		 * @brief Generates the Fetch Shader for a CsShader; typically, this is the CS-stage shader embedded in a CsVsShader.
		 *
		 * This <b>Direct Mapping</b> variant of the function assumes that all vertex buffer slots map directly to the corresponding vertex shader input slots.
		 *
		 * @param[out] fs             Buffer to hold the fetch shader that this method generates. This buffer must be at least as large as the size that computeCsFetchShaderSize() returns.
		 * @param[out] shaderModifier Receives the value that you need to pass to either of GfxContext::setCsVsShaders() or CsVsShader::applyFetchShaderModifiers().
		 * @param[in] csb				The compute shader for which this method generates a CS-stage fetch shader. Must have CsShader::m_version <c> >= 1</c>.
		 * @param[in] instancingData	A pointer to a table describing the index to use to fetch the data for each shader entry. To default always to Vertex Index, pass <c>NULL</c> as this value.
		 * @see computeCsFetchShaderSize()
		 */
		SCE_GNM_API_DEPRECATED_MSG("please ensure instancingData is indexed by VertexInputSemantic::m_semantic and supply numElementsInInstancingData")
		void generateCsFetchShader(void *fs, uint32_t* shaderModifier, const CsShader *csb, const Gnm::FetchShaderInstancingMode *instancingData)
		{
			generateCsFetchShader(fs, shaderModifier, csb, instancingData, instancingData != nullptr ? 256 : 0);
		}

		/** @brief Generates the Fetch Shader for a CsShader, which will typically be the CS-stage shader embedded in a CsVsShader, while allowing for arbitrary remapping of vertex buffer slots to vertex shader input slots.
		 *
		 * This <b>Remapping Table</b> variant of the function uses the specified semantic remapping table to allow the arbitrary remapping of vertex buffer slots to vertex shader input slots.
		 *
		 * @param[out] fs						Buffer to hold the fetch shader that this method generates. This buffer must be at least as large as the size that computeCsFetchShaderSize() returns.
		 * @param[out] shaderModifier			Receives the value that you need to pass to either of GfxContext::setCsVsShaders() or CsVsShader::applyFetchShaderModifiers().
		 * @param[in] csb						The compute shader for which this method generates a CS-stage fetch shader. Must have CsShader::m_version <c> >= 1</c>.
		 * @param[in] semanticRemapTable		A pointer to the semantic remapping table that matches the vertex shader input with the Vertex Buffer definition. This table contains 
		 * 										one element for each vertex buffer slot that may be bound. Each vertex buffer slot's table entry holds the index of the vertex 
		 * 										shader input to associate with that vertex buffer (or <c>0xFFFFFFFF</c>, if the buffer is unused).
		 * @param[in] numElementsInRemapTable	The size of the <c><i>semanticRemapTable</i></c>.
		 *
		 * @see computeCsFetchShaderSize()
		 */
		SCE_GNMX_EXPORT void generateCsFetchShader(void *fs,
												   uint32_t *shaderModifier,
												   const CsShader *csb,
												   const void *semanticRemapTable, const uint32_t numElementsInRemapTable);

		/** @brief Generates the Fetch Shader for a CsShader with instanced fetching (which will typically be the CS-stage shader embedded in a CsVsShader), while allowing for arbitrary remapping of vertex buffer slots to vertex shader input slots.
		 *
		 * This <b>Remapping Table</b> variant of the function uses the specified semantic remapping table to allow the arbitrary remapping of vertex buffer slots to vertex shader input slots.
		 *
		 * @note Semantic remapping is not applied to <c><i>instancingData</i></c> table lookups, and does not affect the required ordering or size of that table.
		 * @note If <c><i>instancingData</i></c> is not <c>NULL</c>, it specifies the index (vertex index or instance id) to use to fetch each semantic input to the vertex shader.
		 *       Non-system-semantic vertex inputs are assigned sequential VertexInputSemantic::m_semantic values starting from <c>0</c> in the order declared in the PSSL source, and
		 *       incremented for each vector, matrix row, or array element, regardless of which elements are used by the shader. Thus, for shaders that do not
		 *       use every declared vertex input, the largest <c>m_semantic</c> value plus 1 may be larger than <c><i>m_numInputSemantics</i></c>.
		 *
		 * @param[out] fs						Buffer to hold the fetch shader that this method generates. This buffer must be at least as large as the size that computeCsFetchShaderSize() returns.
		 * @param[out] shaderModifier			Output value that you need to pass to either of GfxContext::setCsVsShaders() or CsVsShader::applyFetchShaderModifiers() methods.
		 * @param[in] csb						The compute shader for which this method generates a CS-stage fetch shader. Must have CsShader::m_version <c> >= 1</c>.
		 * @param[in] instancingData				A pointer to a table describing the index to use for fetching shader entry data by VertexInputSemantic::m_semantic index. If read at or beyond <c><i>numElementsInInstancingData</i></c>, or if <c>NULL</c>, defaults to Vertex Index.
		 * @param[in] numElementsInInstancingData	The size of the <c><i>instancingData</i></c> table, if not <c>NULL</c>. Generally, this value should match the number of non-system vector vertex inputs in the logical inputs to the vertex shader.
		 * @param[in] semanticRemapTable		A pointer to the semantic remapping table that matches the vertex shader input with the Vertex Buffer definition. This table contains 
		 * 										one element for each vertex buffer slot that may be bound. Each vertex buffer slot's table entry holds the index of the vertex 
		 * 										shader input to associate with that vertex buffer (or <c>0xFFFFFFFF</c>, if the buffer is unused).
		 * @param[in] numElementsInRemapTable	The size of the <c><i>semanticRemapTable</i></c>.
		 *
		 * @see computeCsFetchShaderSize()
		 */
		SCE_GNMX_EXPORT void generateCsFetchShader(void *fs,
												   uint32_t *shaderModifier,
												   const CsShader *csb,
												   const Gnm::FetchShaderInstancingMode *instancingData, const uint32_t numElementsInInstancingData, 
												   const void *semanticRemapTable, const uint32_t numElementsInRemapTable);

		/** @deprecated This function now requires the element count of the <c>instancingData</c> table.
		 * @brief Generates the Fetch Shader for a CsShader, which will typically be the CS-stage shader embedded in a CsVsShader, while allowing for arbitrary remapping of vertex buffer slots to vertex shader input slots.
		 *
		 * This <b>Remapping Table</b> variant of the function uses the specified semantic remapping table to allow for the arbitrary remapping of vertex buffer slots to vertex shader input slots.
		 *
		 * @param[out] fs						Buffer to hold the fetch shader that this method generates. This buffer must be at least as large as the size that computeCsFetchShaderSize() returns.
		 * @param[out] shaderModifier			Receives the value that you need to pass to either of GfxContext::setCsVsShaders() or CsVsShader::applyFetchShaderModifiers().
		 * @param[in] csb						The compute shader for which this method generates a CS-stage fetch shader. Must have CsShader::m_version <c> >= 1</c>.
		 * @param[in] instancingData			A pointer to a table describing the index to use to fetch the data for each shader entry. To default always to Vertex Index, pass <c>NULL</c> as this value.
		 * @param[in] semanticRemapTable		A pointer to the semantic remapping table that matches the vertex shader input with the Vertex Buffer definition. This table contains 
		 * 										one element for each vertex buffer slot that may be bound. Each vertex buffer slot's table entry holds the index of the vertex 
		 * 										shader input to associate with that vertex buffer (or <c>0xFFFFFFFF</c>, if the buffer is unused).
		 * @param[in] numElementsInRemapTable	The size of the <c><i>semanticRemapTable</i></c>.
		 */
		SCE_GNM_API_DEPRECATED_MSG("please ensure instancingData is indexed by VertexInputSemantic::m_semantic and supply numElementsInInstancingData")
		void generateCsFetchShader(void *fs, uint32_t* shaderModifier, const CsShader *csb, const Gnm::FetchShaderInstancingMode *instancingData, const void *semanticRemapTable, const uint32_t numElementsInRemapTable)
		{
			generateCsFetchShader(fs, shaderModifier, csb, instancingData, instancingData != nullptr ? 256 : 0, semanticRemapTable, numElementsInRemapTable);
		}
	}
}
#endif
