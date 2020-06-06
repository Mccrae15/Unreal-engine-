/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 07.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
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
		 * @note If <c><i>instancingData</i></c> is not <c>NULL</c>, it specifies the index (vertex index or instance ID) to use to fetch each semantic input to the vertex shader.
		 *       Non-system-semantic vertex inputs are assigned sequential VertexInputSemantic::m_semantic values starting from <c>0</c> in the order declared in the PSSL source, and 
		 *       incremented for each vector, matrix row, or array element, regardless of which elements are used by the shader. Thus, for shaders that do not
		 *       use every declared vertex input, the largest <c>m_semantic</c> value plus <c>1</c> may be larger than <c><i>m_numInputSemantics</i></c>.
		 *
		 * @param[out] fs				Buffer to hold the fetch shader that this method generates. This buffer must be at least as large as the size that computeVsFetchShaderSize() returns.
		 * @param[out] shaderModifier	Receives a value that you need to pass to either of GfxContext::setVsShader() or VsShader::applyFetchShaderModifier().
		 * @param[in] vsb				A pointer to the vertex shader binary data.
		 * @param[in] instancingData				A pointer to a table describing the index to use for fetching shader entry data by VertexInputSemantic::m_semantic index. If read at or beyond <c><i>numElementsInInstancingData</i></c>, or if <c>NULL</c>, defaults to Vertex Index. 
		 * @param[in] numElementsInInstancingData	The size of the <c><i>instancingData</i></c> table. Generally, this value should match the number of non-system vector vertex inputs in the logical inputs to the vertex shader. If <c><i>instancingData</i></c> is <c>NULL</c>, pass <c>NULL</c> as the value of this parameter.
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
		 * @note If <c><i>instancingData</i></c> is not <c>NULL</c>, it specifies the index (vertex index or instance ID) to use to fetch each semantic input to the vertex shader.
		 *       Non-system-semantic vertex inputs are assigned sequential VertexInputSemantic::m_semantic values starting from <c>0</c> in the order declared in the PSSL source, and
		 *       incremented for each vector, matrix row, or array element, regardless of which elements are used by the shader. Thus, for shaders that do not
		 *       use every declared vertex input, the largest <c>m_semantic</c> value plus <c>1</c> may be larger than <c><i>m_numInputSemantics</i></c>.
		 *
		 * @param[out] fs             			Buffer to hold the fetch shader that this method generates. This buffer must be at least as large as the size that computeVsFetchShaderSize() returns.
		 * @param[out] shaderModifier   		Receives a value that you need to pass to either of GfxContext::setVsShader() or VsShader::applyFetchShaderModifier().
		 * @param[in] vsb                		A pointer to the vertex shader binary data.
		 * @param[in] instancingData				A pointer to a table describing the index to use for fetching shader entry data by VertexInputSemantic::m_semantic index. If read at or beyond <c><i>numElementsInInstancingData</i></c>, or if <c>NULL</c>, defaults to Vertex Index. 
		 * @param[in] numElementsInInstancingData	The size of the <c><i>instancingData</i></c> table. Generally, this value should match the number of non-system vector vertex inputs in the logical inputs to the vertex shader. If <c><i>instancingData</i></c> is <c>NULL</c>, pass <c>NULL</c> as the value of this parameter.
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
		 * @note If <c><i>instancingData</i></c> is not <c>NULL</c>, it specifies the index (vertex index or instance ID) to use to fetch each semantic input to the vertex shader.
		 *       Non-system-semantic vertex inputs are assigned sequential VertexInputSemantic::m_semantic values starting from <c>0</c> in the order declared in the PSSL source, and
		 *       incremented for each vector, matrix row, or array element, regardless of which elements are used by the shader. Thus, for shaders that do not
		 *       use every declared vertex input, the largest <c>m_semantic</c> value plus <c>1</c> may be larger than <c><i>m_numInputSemantics</i></c>.
		 *
		 * @param fs				Buffer to hold the fetch shader that this method generates. This buffer must be at least as large as the size that computeLsFetchShaderSize() returns.
		 * @param shaderModifier	Receives a value which will need to be passed to GfxContext::setLsShader().
		 * @param lsb				A pointer to LDS shader binary data.
		 * @param[in] instancingData				A pointer to a table describing the index to use for fetching shader entry data by VertexInputSemantic::m_semantic index. If read at or beyond <c><i>numElementsInInstancingData</i></c>, or if <c>NULL</c>, defaults to Vertex Index.
		 * @param[in] numElementsInInstancingData	The size of the <c><i>instancingData</i></c> table. Generally, this value should match the number of non-system vector vertex inputs in the logical inputs to the vertex shader. If <c><i>instancingData</i></c> is <c>NULL</c>, pass <c>NULL</c> as the value of this parameter.
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
		 * @note If <c><i>instancingData</i></c> is not <c>NULL</c>, it specifies the index (vertex index or instance ID) to use to fetch each semantic input to the vertex shader.
		 *       Non-system-semantic vertex inputs are assigned sequential VertexInputSemantic::m_semantic values starting from <c>0</c> in the order declared in the PSSL source, and
		 *       incremented for each vector, matrix row, or array element, regardless of which elements are used by the shader. Thus, for shaders that do not
		 *       use every declared vertex input, the largest <c>m_semantic</c> value plus <c>1</c> may be larger than <c><i>m_numInputSemantics</i></c>.
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
		 * @note If <c><i>instancingData</i></c> is not <c>NULL</c>, it specifies the index (vertex index or instance ID) to use to fetch each semantic input to the vertex shader.
		 *       Non-system-semantic vertex inputs are assigned sequential VertexInputSemantic::m_semantic values starting from <c>0</c> in the order declared in the PSSL source, and
		 *       incremented for each vector, matrix row, or array element, regardless of which elements are used by the shader. Thus, for shaders that do not
		 *       use every declared vertex input, the largest <c>m_semantic</c> value plus <c>1</c> may be larger than <c><i>m_numInputSemantics</i></c>.
		 *
		 * @param[out] fs						Buffer to hold the fetch shader that this method generates. This buffer must be at least as large as the size that computeLsFetchShaderSize() returns.
		 * @param[out] shaderModifier			Output value that you need to pass to either of GfxContext::setLsShader() function or LsShader::applyFetchShaderModifier().
		 * @param[in] lsb						Pointer to LDS shader binary data.
		 * @param[in] instancingData			A pointer to a table describing the index to use for fetching shader entry data by <c>VertexInputSemantic::m_semantic</c> index. If read at or beyond <c><i>numElementsInInstancingData</i></c>, or if <c>NULL</c>, defaults to Vertex Index. 
		 * @param[in] numElementsInInstancingData	The size of the <c><i>instancingData</i></c> table. Generally, this value should match the number of non-system vector vertex inputs in the logical inputs to the vertex shader. If <c><i>instancingData</i></c> is <c>NULL</c>, pass <c>NULL</c> as the value of this parameter.
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
		 * @note If <c><i>instancingData</i></c> is not <c>NULL</c>, it specifies the index (vertex index or instance ID) to use to fetch each semantic input to the vertex shader.
		 *       Non-system-semantic vertex inputs are assigned sequential VertexInputSemantic::m_semantic values starting from <c>0</c> in the order declared in the PSSL source, and
		 *       incremented for each vector, matrix row, or array element, regardless of which elements are used by the shader. Thus, for shaders that do not
		 *       use every declared vertex input, the largest <c>m_semantic</c> value plus <c>1</c> may be larger than <c><i>m_numInputSemantics</i></c>.
		 *
		 * @param[out] fs					Buffer to hold the fetch shader that this method generates. This buffer must be at least as large as the size that computeEsFetchShaderSize() returns.
		 * @param[out] shaderModifier		Receives the value that you need to pass to either of GfxContext::setEsShader() or EsShader::applyFetchShaderModifier().
		 * @param[in] esb					Pointer to the export shader binary data.
		 * @param[in] instancingData				A pointer to a table describing the index to use for fetching shader entry data by VertexInputSemantic::m_semantic index. If read at or beyond <c><i>numElementsInInstancingData</i></c>, or if <c>NULL</c>, defaults to Vertex Index.
		 * @param[in] numElementsInInstancingData	The size of the <c><i>instancingData</i></c> table. Generally, this value should match the number of non-system vector vertex inputs in the logical inputs to the vertex shader. If <c><i>instancingData</i></c> is <c>NULL</c>, pass <c>NULL</c> as the value of this parameter.
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

		/** @brief Generates the Fetch Shader for an ES-stage vertex shader while allowing for arbitrary remapping of vertex buffer slots to vertex shader input slots.
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

		/** @brief Generates the Fetch Shader for an ES-stage vertex shader with instanced fetching, while allowing for arbitrary remapping of vertex buffer slots to vertex shader input slots.
		 *
		 * This <b>Remapping Table</b> variant of the function uses the specified semantic remapping table to allow the arbitrary remapping of vertex buffer slots to vertex shader input slots.
		 *
		 * @note Semantic remapping is not applied to <c><i>instancingData</i></c> table lookups, and does not affect the required ordering or size of that table.
		 * @note If <c><i>instancingData</i></c> is not <c>NULL</c>, it specifies the index (vertex index or instance ID) to use to fetch each semantic input to the vertex shader.
		 *       Non-system-semantic vertex inputs are assigned sequential VertexInputSemantic::m_semantic values starting from <c>0</c> in the order declared in the PSSL source, and
		 *       incremented for each vector, matrix row, or array element, regardless of which elements are used by the shader. Thus, for shaders that do not
		 *       use every declared vertex input, the largest <c>m_semantic</c> value plus <c>1</c> may be larger than <c><i>m_numInputSemantics</i></c>.
		 *
		 * @param[out] fs						Buffer to hold the fetch shader that this method generates. This buffer must be at least as large as the size that computeEsFetchShaderSize() returns.
		 * @param[out] shaderModifier			Receives the value that you need to pass to either of GfxContext::setEsShader() or EsShader::applyFetchShaderModifier().
		 * @param[in] esb						Pointer to the export shader binary data.
		 * @param[in] instancingData			A pointer to a table describing the index to use for fetching shader entry data by VertexInputSemantic::m_semantic index. If read at or beyond <c><i>numElementsInInstancingData</i></c>, or if <c>NULL</c>, defaults to Vertex Index.
		 * @param[in] numElementsInInstancingData	The size of the <c><i>instancingData</i></c> table. Generally, this value should match the number of non-system vector vertex inputs in the logical inputs to the vertex shader. If <c><i>instancingData</i></c> is <c>NULL</c>, pass <c>NULL</c> as the value of this parameter.
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

		 * @brief Generates the Fetch Shader for an ES-stage compute shader while allowing for arbitrary remapping of vertex buffer slots to vertex shader input slots.
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
	}
}
#endif
