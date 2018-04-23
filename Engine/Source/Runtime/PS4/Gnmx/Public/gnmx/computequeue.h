/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef _SCE_GNMX_COMPUTEQUEUE_H_
#define _SCE_GNMX_COMPUTEQUEUE_H_

#include <gnm.h>
#include "computecontext.h"

namespace sce
{
	namespace Gnmx
	{
		/** @brief Encapsulates the "setup and submit" functionality of compute queues in a single C++ object. */
		class SCE_GNMX_EXPORT ComputeQueue
		{
		public:
			/** @brief Defines the results of a submission to a compute queue.
			*/
			typedef enum SubmissionStatus
			{
				kSubmitOK			   = 0,				///< The submission was successful.		
				kSubmitFailQueueIsFull = 0x80000001		///< The submission failed because the queue was full.
			} SubmissionStatus;


			/** @brief  Initializes a compute queue object.
				@param[in]  pipeId   Pipe to use for this compute queue (<c><i>pipeId</i></c>: 0 -> 6).
				@param[in]  queueId  Queue to use for this compute queue (<c><i>queueId</i></c>: 0 -> 7).
			*/
			void initialize(uint32_t pipeId, uint32_t queueId);


			/** @brief  Resets the compute queue’s pointer to the beginning of the queue.
			*/
			void reset();
			/** @brief  Resets the compute queue’s pointer to the beginning of the queue.
				@param[in] workloadId	The id of the workload corresponding to this reset. @sa Gnm::beginWorkload()
			*/
			void reset(uint64_t workloadId);

#ifdef __ORBIS__
			/** @brief  Maps a ring buffer address to the compute queue.
				@param[in]  ringBaseAddr Base address of the ring buffer. This address must be 256-byte aligned.
				@param[in]  ringSizeInDW     Size of the ring buffer in <c>DWORD</c>s. The ring size must be a power of two, with a minimum value of 128 <c>DWORD</c>s.
				@param[in]  readPtrAddr  Memory address at which the hardware is going to write the current 32-bit 'read pointer' value. This address must be 4-byte aligned.
				@return Success or failure to map the address.
			*/
			bool map(void *ringBaseAddr, uint32_t ringSizeInDW, void *readPtrAddr);

			/** @brief Unmaps the queue */
			void unmap();

			/** @brief Tests to see if this queue is currently mapped. 
				@return Returns <c>true</c> if this queue is currently mapped. 
			*/
			bool isMapped() const { return (m_vqueueId != 0); }

			/** @brief Submits a compute context into this queue.

				@param[in] cmpc  Pointer to the compute context to execute.
				@return Status of the submission.
				@note When called after Gnm::submitDone(), this call may block the CPU until the GPU is idle. You can use Gnm::areSubmitsAllowed() to determine whether a call to submitDone() will block the CPU.
				@see Gnm::areSubmitsAllowed()
			*/
			SubmissionStatus submit(ComputeContext *cmpc);

			/** @brief Submits a Gnm::DispatchCommandBuffer into this queue.

				@param[in] dcb  Pointer to the dispatch command buffer to execute.
				@return Status of the submission.
				@note When called after Gnm::submitDone(), this call may block the CPU until the GPU is idle. You can use Gnm::areSubmitsAllowed() to determine whether a call to submitDone() will block the CPU.
				@see Gnm::areSubmitsAllowed()
			*/
			inline SubmissionStatus submit(GnmxDispatchCommandBuffer *dcb)
			{
				return submit(dcb->m_beginptr, (uint32_t)(dcb->m_cmdptr - dcb->m_beginptr)*4);
			}

			/** @brief Submits a Gnm::DispatchCommandBuffer into this queue.

				@param[in] pDcb  Pointer to the dispatch command buffer commands to execute.
				@param[in] sizeInBytes  Size in bytes of the dispatch command buffer commands to execute.
				@return Status of the submission.
				@note When called after Gnm::submitDone(), this call may block the CPU until the GPU is idle. You can use Gnm::areSubmitsAllowed() to determine whether a call to submitDone() will block the CPU.
				@see Gnm::areSubmitsAllowed()
			*/
			inline SubmissionStatus submit(void const *pDcb, uint32_t sizeInBytes)
			{
				return submit(1, &pDcb, &sizeInBytes);
			}

			/** @brief Submits an array of dispatch command buffers into this queue.

				@param[in] numBuffers  Number of dispatch command buffers to insert into this queue.
				@param[in] apDcb  Array of <c><i>numBuffers</i></c> pointers to command buffers to execute.
				@param[in] aDcbSizes  Array of <c><i>numBuffers</i></c> sizes in bytes for the command buffers to execute.
				@return Status of the submission.
				@note When called after Gnm::submitDone(), this call may block the CPU until the GPU is idle. You can use Gnm::areSubmitsAllowed() to determine whether a call to submitDone() will block the CPU.
				@see Gnm::areSubmitsAllowed()
			*/
			SubmissionStatus submit(uint32_t numBuffers, void const*const* apDcb, uint32_t const *aDcbSizes);

			/** @brief Submits a compute context into this queue.

				@param[in] workloadId	The id of the workload corresponding to this context. @sa Gnm::beginWorkload()
				@param[in] cmpc  Pointer to the compute context to execute.
				@return Status of the submission.
			*/
			SubmissionStatus submit(uint64_t workloadId, ComputeContext *cmpc);

			/** @brief Submits a Gnm::DispatchCommandBuffer into this queue.

				@param[in] workloadId	The id of the workload corresponding to this buffer. @sa Gnm::beginWorkload()
				@param[in] dcb  Pointer to the dispatch command buffer to execute.
				@return Status of the submission.
			*/
			inline SubmissionStatus submit(uint64_t workloadId, GnmxDispatchCommandBuffer *dcb)
			{
				return submit(workloadId, dcb->m_beginptr, (uint32_t)(dcb->m_cmdptr - dcb->m_beginptr)*4);
			}

			/** @brief Submits a Gnm::DispatchCommandBuffer into this queue.

				@param[in] workloadId	The id of the workload corresponding to this buffer. @sa Gnm::beginWorkload()
				@param[in] pDcb  Pointer to the dispatch command buffer commands to execute.
				@param[in] sizeInBytes  Size (in bytes) of the dispatch command buffer commands to execute.
				@return Status of the submission.
			*/
			inline SubmissionStatus submit(uint64_t workloadId, void const *pDcb, uint32_t sizeInBytes)
			{
				return submit(workloadId, 1, &pDcb, &sizeInBytes);
			}

			/** @brief Submits an array of dispatch command buffers into this queue.

				@param[in] workloadId	The id of the workload corresponding to these buffers. @sa Gnm::beginWorkload()
				@param[in] numBuffers  Number of dispatch command buffers to insert into this queue.
				@param[in] apDcb  Array of <c><i>numBuffers</i></c> pointers to the command buffers to execute.
				@param[in] aDcbSizes  Array of <c><i>numBuffers</i></c> sizes (in bytes) for the command buffers to execute.
				@return Status of the submission.
			*/
			SubmissionStatus submit(uint64_t workloadId, uint32_t numBuffers, void const*const* apDcb, uint32_t const *aDcbSizes);
#endif // __ORBIS__

			uint32_t  m_pipeId;				  ///< Pipe ID of this compute queue.
			uint32_t  m_queueId;			  ///< Queue ID of this compute queue.
			
			uint32_t  m_vqueueId;			  ///< Virtual queue (vqueue) ID used to unmap, submit, and so on.
			uint32_t  m_reserved;			  ///< reserved (must be 0)
			volatile uint32_t* m_readPtrAddr; ///< Read pointer address.

#if !defined(DOXYGEN_IGNORE)
			// Internal use only.
			GnmxDispatchCommandBuffer m_dcbRoot;
#endif // !defined(DOXYGEN_IGNORE)
		};
	};
}

#endif /* _SCE_GNMX_COMPUTEQUEUE_H_ */
