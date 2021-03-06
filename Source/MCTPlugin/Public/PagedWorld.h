#pragma once

#include "UnrealFastNoisePlugin.h"
#include "UnrealFastNoisePlugin/Public/UFNNoiseGenerator.h"
#include "PolyVox/PagedVolume.h"
#include "PolyVox/MaterialDensityPair.h"
#include "PolyVox/Vector.h"
#include "PolyVox/MarchingCubesSurfaceExtractor.h"
#include "GameFramework/Actor.h"
#include "RuntimeMeshComponent.h"
#include "LevelDatabase.h"
#include "CoreMinimal.h"
#include "VoxelNetThreads.h"
#include "Networking/Public/Common/TcpListener.h"
#include "Structs.h"
#include "StorageProviderBase.h"
#include "WorldGeneratorBase.h"
#include "PagedWorld.generated.h"

//cleaned up

class APagedRegion;
class UTerrainPagingComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FVoxelWorldUpdate, class AActor*, CauseActor, const FIntVector, voxelLocation, const uint8, oldMaterial, const uint8, newMaterial, const bool, bShouldDrop);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVoxelNetHandshake,const int64, cookie);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPreSaveWorld,const bool, isQuitting);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPostSaveWorld,const bool, isQuitting);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRegionGenerated,const FIntVector, Region);


#ifdef WORLD_TICK_TRACKING
	DECLARE_STATS_GROUP(TEXT("VoxelWorld"), STATGROUP_VoxelWorld, STATCAT_Advanced);
#endif

UCLASS()
	class MCTPLUGIN_API APagedWorld : public AActor {
	GENERATED_BODY()

public:
	APagedWorld();
	~APagedWorld();

protected:
	void BeginPlay() override;
	void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	void VoxelUpdatesTick();
	void WorldNewRegionsTick();
	void VoxelNetClientTick();
	void VoxelNetServerTick();
	void PostInitializeComponents() override;
	void BeginDestroy() override;

	// PIE is not saving properly it just exits immediately without cleaning anything up...
	void SaveAndShutdown();

	bool bHasStarted = false;
	bool bHasShutdown = false;

public:
	void Tick(float DeltaTime) override;
	//debug
	UFUNCTION(Category = "Voxel World|Saving", BlueprintImplementableEvent) void OnRegionError(FIntVector Region);

	FQueuedThreadPool *VoxelWorldThreadPool;
	
	/* World */
	UFUNCTION(Category = "Voxel World", BlueprintCallable) APagedRegion* getRegionAt(FIntVector pos);
	UFUNCTION(Category = "Voxel World|Coordinates", BlueprintPure) static FIntVector VoxelToRegionCoords(FIntVector VoxelCoords);
	UFUNCTION(Category = "Voxel World|Coordinates", BlueprintPure) static FIntVector WorldToVoxelCoords(FVector WorldCoords);
	UFUNCTION(Category = "Voxel World|Coordinates", BlueprintPure) static FVector VoxelToWorldCoords(FIntVector VoxelCoords);
	UFUNCTION(Category = "Voxel World", BlueprintCallable, NetMulticast, Reliable) void Multi_ModifyVoxel(FIntVector VoxelLocation, uint8 Radius, uint8 Material, uint8 Density, AActor* cause = nullptr, bool bIsSpherical = false, bool bShouldDrop = true);
	UFUNCTION(Category = "Voxel World", BlueprintCallable, Server, Reliable, WithValidation) void Server_ModifyVoxel(FIntVector VoxelLocation, uint8 Radius, uint8 Material, uint8 Density, AActor* cause = nullptr, bool bIsSpherical = false, bool bShouldDrop = true);
	UPROPERTY(BlueprintAssignable, Category="Voxel Update Event") FVoxelWorldUpdate VoxelWorldUpdate_Event;
	UPROPERTY(BlueprintReadWrite, EditAnywhere) bool bUseAsyncCollision = true;
	TSharedPtr<PolyVox::PagedVolume<PolyVox::MaterialDensityPair88>> VoxelVolume;
	FCriticalSection VolumeMutex; // lock for VoxelVolume
	TQueue<FVoxelUpdate, EQueueMode::Mpsc> voxelUpdateQueue;
	TSet<FIntVector> ForceLoadedRegions; 

	/* Database */
	UFUNCTION(Category = "Voxel World|Database", BlueprintCallable) void ConnectToDatabase(FString Name);
	StorageProviderBase* WorldStorageProvider;

	/* Memory */
	UFUNCTION(Category = "Voxel World|Volume Memory", BlueprintCallable) int32 getVolumeMemoryBytes() const;
	UFUNCTION(Category = "Voxel World|Volume Memory", BlueprintCallable) void Flush() const;

	/* Startup Helpers */
	UFUNCTION(Category = "Voxel World|Launch", BlueprintCallable) void LaunchServer();
	UFUNCTION(Category = "Voxel World|Launch", BlueprintCallable) void LaunchSingleplayer();
	UFUNCTION(Category = "Voxel World|Launch", BlueprintCallable) void LaunchClient(FString Host, int32 Port = 0);

	/* Saving */
	UFUNCTION(Category = "Voxel World|Saving", BlueprintCallable) void ForceSaveWorld() ;
	UFUNCTION(Category = "Voxel World|Saving", BlueprintImplementableEvent) void PreSaveWorld();
	UFUNCTION(Category = "Voxel World|Saving", BlueprintImplementableEvent) void PostSaveWorld();
	UFUNCTION(Category = "Voxel World|Saving", BlueprintCallable) void SaveAllDataForRegions(TSet<FIntVector> Regions);
	UFUNCTION(Category = "Voxel World|Saving", BlueprintCallable) void LoadAllDataForRegions(TSet<FIntVector> Regions);
	UPROPERTY(BlueprintAssignable, Category="Voxel World|Saving") FPreSaveWorld PreSaveWorld_Event;
	UPROPERTY(BlueprintAssignable, Category="Voxel World|Saving") FPostSaveWorld PostSaveWorld_Event;

	/* Generation */
	UFUNCTION(BlueprintImplementableEvent) const TArray<UUFNNoiseGenerator*> GetNoiseGeneratorArray(); 
	UFUNCTION(Category = "Voxel World|Generation", BlueprintCallable) void BeginWorldGeneration(FIntVector RegionCoords);
	UFUNCTION(Category = "Voxel World|Generation", BlueprintCallable, BlueprintAuthorityOnly) float GetHeightmapZ(int32 VoxelX, int32 VoxelY, uint8 HeightmapIndex = 0);
	UFUNCTION(Category = "Voxel World|Generation", BlueprintCallable) void PrefetchRegionsInRadius(FIntVector pos, int32 radius) const;
	UFUNCTION(Category = "Voxel World|Generation", BlueprintCallable) void RegisterPagingComponent(UTerrainPagingComponent* pagingComponent);
	UFUNCTION(Category = "Voxel World|Generation", BlueprintCallable) void PagingComponentTick();
	UFUNCTION(Category = "Voxel World|Generation", BlueprintCallable) void UnloadRegionsExcept(TSet<FIntVector> loadedRegions);
	UPROPERTY(Category = "Voxel World|Generation", BlueprintReadOnly, VisibleAnywhere) TMap<FIntVector, APagedRegion*> regions;
	UPROPERTY(Category = "Voxel World|Generation", BlueprintReadOnly, VisibleAnywhere) TArray<UTerrainPagingComponent*> pagingComponents;
	UPROPERTY(Category = "Voxel World|Generation", BlueprintReadOnly, VisibleAnywhere) int32 remainingRegionsToGenerate = 0;
	UPROPERTY(BlueprintAssignable, Category="Voxel World|Generation") FRegionGenerated RegionGenerated_Event;
	//WorldGeneratorBase * WorldGenerationProvider;
	TQueue<FWorldGenerationTaskOutput, EQueueMode::Mpsc> worldGenerationQueue;
	float PagingComponentTickTimer = 0;
	UPROPERTY(Category = "Voxel World|Generation", BlueprintReadWrite, EditAnywhere) float PagingComponentTickRate = 1.f;

	/* Rendering */
	UPROPERTY(Category = "Voxel World|Rendering", BlueprintReadWrite, EditAnywhere) bool bRenderMarchingCubes = false;
	UPROPERTY(Category = "Voxel World|Rendering", BlueprintReadWrite, EditAnywhere) TArray<UMaterialInterface*> TerrainMaterials;
	UFUNCTION(Category = "Voxel World|Rendering", BlueprintCallable) void QueueRegionRender(FIntVector pos);
	UFUNCTION(Category = "Voxel World|Rendering", BlueprintCallable) void MarkRegionDirtyAndAdjacent(FIntVector pos);
	TSet<FIntVector> dirtyRegions;// region keys which need redrawn & recooked; either because their voxels were modified or because they were just created
	TQueue<FExtractionTaskOutput, EQueueMode::Mpsc> extractionQueue;

	/* Networking */
	UPROPERTY(Category = "Voxel World|Networking", BlueprintReadWrite, EditAnywhere) bool bIsVoxelNetServer = false;
	UPROPERTY(Category = "Voxel World|Networking", BlueprintReadWrite, EditAnywhere) bool bIsVoxelNetSingleplayer = false;
	UPROPERTY(BlueprintAssignable, Category="VoxelNet Handshake Event") FVoxelNetHandshake VoxelNetHandshake_Event;

	/* Voxelnet Server */
	UFUNCTION(Category = "Voxel World|Networking|Server", BlueprintCallable)bool VoxelNetServer_StartServer();
	UFUNCTION(Category = "Voxel World|Networking|Server", BlueprintCallable) void RegisterPlayerWithCookie(APlayerController* player, int64 cookie);
	bool VoxelNetServer_SendPacketsToPagingComponent(UTerrainPagingComponent*& pager, TArray<TArray<uint8>> packets);
	TMap<FIntVector, TArray<uint8>> VoxelNetServer_regionPackets;
	TQueue<FPacketTaskOutput, EQueueMode::Mpsc> VoxelNetServer_packetQueue;
	bool VoxelNetServer_OnConnectionAccepted(FSocket* socket, const FIPv4Endpoint& endpoint);
	TSharedPtr<FTcpListener> VoxelNetServer_ServerListener;
	TArray<TSharedPtr<VoxelNetThreads::VoxelNetServer>> VoxelNetServer_VoxelServers;
	TArray<FRunnableThread*> VoxelNetServer_ServerThreads;
	// after handshake we associate playercontrollers with server threads, for uploading regions
	//	Server sends handshake
	//	Client receives and sends the cookie thru their playercontroller to the world Calling ServerRegisterPlayerWithCookie
	//	RegisterPlayerWithCookie takes a calling playercontroller and a cookie, looks up the cookie in map and associates Player:Socket
	TMap<int64, TSharedPtr<VoxelNetThreads::VoxelNetServer>> VoxelNetServer_SentHandshakes;
	TMap<APlayerController*, TSharedPtr<VoxelNetThreads::VoxelNetServer>> VoxelNetServer_PlayerVoxelServers;

	/* Voxelnet Client */
	UFUNCTION(Category = "Voxel World|Networking|Client", BlueprintCallable)bool VoxelNetClient_ConnectToServer(FString Host, int32 Port = 0);
	UFUNCTION(Category = "Voxel World|Networking|Client", BlueprintCallable)int32 VoxelNetClient_GetPendingRegionDownloads() const;
	TSharedPtr<VoxelNetThreads::VoxelNetClient> VoxelNetClient_VoxelClient;
	FRunnableThread* VoxelNetClient_ClientThread;
};

class WorldPager : public PolyVox::PagedVolume<PolyVox::MaterialDensityPair88>::Pager {
public:
	WorldPager(APagedWorld* World);

	virtual ~WorldPager() {
	};

	void pageIn(const PolyVox::Region& region,
	            PolyVox::PagedVolume<PolyVox::MaterialDensityPair88>::Chunk* pChunk) override;
	void pageOut(const PolyVox::Region& region,
	             PolyVox::PagedVolume<PolyVox::MaterialDensityPair88>::Chunk* pChunk) override;

	APagedWorld* world;
};

