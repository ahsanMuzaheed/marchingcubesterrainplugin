#pragma once
#include "WorldGenInterpreters.h"
#include "Config.h"
#include "PagedWorld.h"

namespace WorldGenThreads {
	////////////////////////////////////////////////////////////////////////
	class RegionGenerationTask : public FNonAbandonableTask {
		friend class FAutoDeleteAsyncTask<RegionGenerationTask>;
		APagedWorld* world;
		FIntVector lower;
	public:
		RegionGenerationTask(APagedWorld* world,
		                     FIntVector lower/*, PolyVox::PagedVolume<PolyVox::MaterialDensityPair88>::Chunk * pChunk*/):world(world),lower(lower) {

		}

		FORCEINLINE TStatId GetStatId() const {
			RETURN_QUICK_DECLARE_CYCLE_STAT(RegionGenerationTask, STATGROUP_ThreadPoolAsyncTasks);
		}

		void DoWork() {
			const TArray<UUFNNoiseGenerator*> noise = world->GetNoiseGeneratorArray();

			FWorldGenerationTaskOutput output;
			output.pos = lower;

			// generate
			for (int32 x = 0; x < REGION_SIZE; x++) {
				for (int32 y = 0; y < REGION_SIZE; y++) {
					for (int32 z = 0; z < REGION_SIZE; z++) {
						if (noise.Num() == 0)
							return; // this happens if the game quits during worldgen
						// todo save function ptr to interp as param that way we can change them on the fly
						output.voxel[x][y][z] = WorldGen::Interpret_New(x + lower.X, y + lower.Y, z + lower.Z, noise);
					}
				}
			}

			// i can do multiple stages of generation here

			world->worldGenerationQueue.Enqueue(output);
		}
	};

	////////////////////////////////////////////////////////////////////////
};
