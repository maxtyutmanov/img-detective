#pragma once

#include "common/CommonInternal.h"
#include "core/IndexManager.h"

namespace ImgDetective {
namespace Core {

	//performs linear search in an unsorted set of features.
	//reads features packet by packet from some data source
	CONCRETE SEALED class LinearSearchIM : public IndexManager {
	public:
		CTOR LinearSearchIM(EXCLUSIVE IndexStorage* storage, Feature::type_id_t featureTypeId);

		virtual IndexSeekResult* Search(REF Feature& f, const REF ImgQuery& query) const;
	private:
		void ProcessPacket(REF Feature& exampleFeature, REF IndexNode::col_p_t& packet, REF IndexSeekResult& result) const;
	};

}
}