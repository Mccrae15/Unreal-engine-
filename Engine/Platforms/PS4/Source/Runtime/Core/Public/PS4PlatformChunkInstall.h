// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include <playgo.h>
#include "GenericPlatform/GenericPlatformChunkInstall.h"

struct FPS4ChunkStatus
{
	ScePlayGoLocus Locus;
};


/**
 * PS4 implementation of FGenericPlatformChunkInstall.
 */
class CORE_API FPS4ChunkInstall : public FGenericPlatformChunkInstall
{
public:

	FPS4ChunkInstall();
	virtual ~FPS4ChunkInstall();

	/** 
	 * Check if a given reporting type is supported.
	 *
	 * @param ReportType Enum specifying how progress is reported.
	 * @return true if reporting type is supported on the current platform.
	 */
	virtual bool GetProgressReportingTypeSupported(EChunkProgressReportingType::Type ReportType) override;	

	/**
	 * Get the current install progress of a chunk.  Let the user specify report type for platforms that support more than one.
	 *
	 * @param PakchunkIndex The id of the chunk to check.
	 * @param ReportType The type of progress report you want.
	 * @return A value whose meaning is dependent on the ReportType param.
	 */
	virtual float GetChunkProgress( uint32 PakchunkIndex, EChunkProgressReportingType::Type ReportType ) override;

	/**
	 * Inquire about the priority of chunk installation vs. game IO.
	 *
	 * @return Paused, low or high priority.
	 */
	virtual EChunkInstallSpeed::Type GetInstallSpeed() override;

	/**
	 * Specify the priority of chunk installation vs. game IO.
	 *
	 * @param InstallSpeed Pause, low or high priority.
	 * @return false if the operation is not allowed, otherwise true.
	 */
	virtual bool SetInstallSpeed( EChunkInstallSpeed::Type InstallSpeed ) override;

	/**
	 * Debug function to start transfer of next chunk in the transfer list.  When in PlayGo HostFS emulation, this is the only
	 * way moving out of network-local will happen for a chunk.	 Does nothing in a shipping build.
	 *
	 * @return true if the operation succeeds.
	 */
	virtual bool DebugStartNextChunk( ) override;

	/**
	* Request a delegate callback on chunk install completion. Request may not be respected.
	* @param ChunkID		The id of the chunk of interest.
	* @param Delegate		The delegate when the chunk is installed.
	* @return				False if the delegate was not registered or the chunk is already installed. True on success.
	*/
	virtual FDelegateHandle SetChunkInstallDelgate(uint32 ChunkID, FPlatformChunkInstallCompleteDelegate Delegate) override;

	/**
	* Remove a delegate callback on chunk install completion.
	* @param ChunkID		The id of the chunk of interest.
	* @param Delegate		The delegate to remove.
	* @return				False if the delegate was not registered with a call to SetChunkInstallDelgate. True on success.
	*/
	virtual void RemoveChunkInstallDelgate(uint32 ChunkID, FDelegateHandle Delegate) override;
	
	bool Tick(float DeltaSeconds);

	/**
	* Check whether current platform supports intelligent chunk installation
	* @return				whether Intelligent Install is supported
	*/
	virtual bool SupportsIntelligentInstall() override
	{
		return true;
	}

	/**
	* Check whether installation of chunks are pending
	* @return				whether installation task has been kicked
	*/
	virtual bool IsChunkInstallationPending(const TArray<FCustomChunk>& CustomChunks);

	/**
	* Install user-defined language chunks
	* @return		Whether user-define language chunk mask has been successfully set
	*/
	virtual bool InstallChunks(const TArray<FCustomChunk>& CustomChunks) override;

	/**
	* Uninstall user-defined language chunks
	* @return		Whether user-define language chunk mask has been successfully cleared
	*/
	virtual bool UninstallChunks(const TArray<FCustomChunk>& CustomChunks) override;

	/**
	 * Get the current location of a chunk with pakchunk id.
	 * @param PakchunkIndex		The id of the pak chunk.
	 * @return				Enum specifying whether the chunk is available to use, waiting to install, or does not exist.
	 **/
	virtual EChunkLocation::Type GetPakchunkLocation(int32 PakchunkIndex) override;

	/**
	 * Hint to the installer that we would like to prioritize a specific chunk
	 *
	 * @param PakchunkIndex The id of the chunk to prioritize.
	 * @param Priority The priority for the chunk.
	 * @return false if the operation is not allowed or the chunk doesn't exist, otherwise true.
	 */
	virtual bool PrioritizePakchunk(int32 PakchunkIndex, EChunkPriority::Type Priority) override;

	/**
	 * Allow an external system to notify that a particular pakchunk has become available
	 * Initial use-case is for dynamically encrypted pak files to signal to the outside world that
	 * it has become available.
	 *
	 * @param InPakchunkIndex - Index of the pakchunk that has just become available
	 */
	virtual void ExternalNotifyChunkAvailable(uint32 InPakchunkIndex) override;

private:

	//Adds the installer to the core ticker.
	void AddToTicker();

	//Removes the installer from the core ticker.
	void RemoveFromTicker();	

	/**
	 * Get the ETA for complete download of a chunk.
	 *
	 * @param ChunkID The id of the chunk to check.
	 * @return Percent downloaded in 99.99 format.
	 */
	float GetChunkETA(uint32 ChunkID);

	/**
	 * Get the current percent downloaded of a chunk.
	 *
	 * @param ChunkID The id of the chunk to check.
	 * @return Percent downloaded in 99.99 format.
	 */
	float GetChunkPercentComplete(uint32 ChunkID);

	/** Unload PlayGo and free everything up */
	void ShutDown();

	/** Mount Paks */
	bool MountPaks(uint32 ChunkID);

	/**
	* Set PlayGo language mask for platform
	* @param LanguageMask	Language mask to set
	* @return				Whether language mask has been successfully set
	*/
	bool SetPlayGoLanguageMask(uint64 LanguageMask);

	/**
	* Clear PlayGo language mask for platform
	* @param LanguageMask	Language mask to clear
	* @return				Whether language mask has been successfully cleared
	*/
	bool ClearPlayGoLanguageMask(uint64 LanguageMask);

	/**
	 * Get the current location of a chunk.
	 *
	 * @param ChunkID The id of the chunk to check.
	 * @return Enum specifying whether the chunk is available to use, waiting to install, or does not exist.
	 */
	virtual EChunkLocation::Type GetChunkLocation(uint32 ChunkID) override;

	/**
	 * Hint to the installer that we would like to prioritize a specific chunk
	 *
	 * @param ChunkID The id of the chunk to prioritize.
	 * @param Priority The priority for the chunk.
	 * @return false if the operation is not allowed or the chunk doesn't exist, otherwise true.
	 */
	virtual bool PrioritizeChunk(uint32 ChunkID, EChunkPriority::Type Priority) override;

private:	

	DECLARE_MULTICAST_DELEGATE_OneParam(FPlatformChunkInstallCompleteMultiDelegate, uint32);

	/** map of chunk IDs to the cached status of that chunk */
	TMap<uint32, FPS4ChunkStatus> ChunkStatus;
	TSet<uint32> MountedChunks;
	TMap<uint32, FPlatformChunkInstallCompleteMultiDelegate> DelegateMap;

	/** Delegate for callbacks to Tick */
	FDelegateHandle TickHandle;

	/** handle to the PlayGo package. */
	ScePlayGoHandle PlayGoHandle = 0;

	/** Whether we have a valid package. */
	bool bIsPlayGoPackage = false;
};