#include "core/SearchSystem.h"
#include "utils/MemoryUtils.h"
#include "utils/ContractUtils.h"

using namespace std;

namespace ImgDetective {
namespace Core {

    SearchSystem::SearchSystem(EXCLUSIVE IFeatureRepository* featureRepo, FeatureExtractor::col_p_t featureExtractors) {
		Utils::Contract::AssertNotNull(featureRepo);
		Utils::Contract::AssertNotEmpty(featureExtractors);

		this->featureRepo = featureRepo;
		this->featureExtractors = featureExtractors;
	}

	SearchSystem::~SearchSystem() {
		Utils::Memory::SafeDelete(featureRepo);
		Utils::Memory::SafeDeleteCollectionOfPointers(featureExtractors);
	}

	imgid_col_t SearchSystem::GetSimilarImgs(ImgQuery query) {
		ImgInfo* imgInfo;

		try {
			//constuct internal representation of example image
			imgInfo = ImgInfo::Create(query.example);

			Feature::col_p_t extractedFeatures = ExtractFeatures(*imgInfo, query);
			imgid_col_t result = featureRepo->GetSimilarImgs(extractedFeatures, query);

			Utils::Memory::SafeDelete(imgInfo);

			return result;
		}
		catch (...) {
			try {
				Utils::Memory::SafeDelete(imgInfo);
			}
			catch (...) {
				//TODO: logging
			}
			throw;
		}
	}

	IFeature::col_p_t SearchSystem::ExtractFeatures(ImgInfo& imgInfo, ImgQuery query) {
		FeatureExtractor::col_p_t::iterator it;
		IFeature::col_p_t result;

		for (it = featureExtractors.begin(); it != featureExtractors.end(); ++it) {
			IFeature* f = (*it)->ExtractFrom(imgInfo);
			result.push_back(f);
		}

		return result;
	}

}
}