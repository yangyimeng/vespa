// Copyright 2016 Yahoo Inc. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
#include <vespa/fastos/fastos.h>
#include <vespa/log/log.h>
LOG_SETUP(".proton.feedoperation.pruneremoveddocumentsoperation");

#include "pruneremoveddocumentsoperation.h"
#include <vespa/vespalib/util/stringfmt.h>

using document::DocumentTypeRepo;
using search::DocumentIdT;
using storage::spi::Timestamp;
using vespalib::make_string;

namespace proton {

PruneRemovedDocumentsOperation::PruneRemovedDocumentsOperation()
    : RemoveDocumentsOperation(FeedOperation::PRUNE_REMOVED_DOCUMENTS),
      _subDbId(0)
{
}


PruneRemovedDocumentsOperation::
PruneRemovedDocumentsOperation(DocumentIdT docIdLimit,
                               uint32_t subDbId)
    : RemoveDocumentsOperation(FeedOperation::PRUNE_REMOVED_DOCUMENTS),
      _subDbId(subDbId)
{
    LidVectorContext::SP lidsToRemove(new LidVectorContext(docIdLimit));
    setLidsToRemove(lidsToRemove);
}


void
PruneRemovedDocumentsOperation::serialize(vespalib::nbostream &os) const
{
    LOG(debug, "serialize(): %s", toString().c_str());
    os << _subDbId;
    assert(_lidsToRemoveMap.size() == 1);
    assert(_lidsToRemoveMap.begin()->first == _subDbId);
    serializeLidsToRemove(os);
}


void
PruneRemovedDocumentsOperation::deserialize(vespalib::nbostream &is,
                                            const DocumentTypeRepo &)
{
    is >> _subDbId;
    deserializeLidsToRemove(is);
}

vespalib::string PruneRemovedDocumentsOperation::toString() const {
    LidVectorContext::SP lids = getLidsToRemove();
    return make_string("PruneRemovedDocuments(limitLid=%zu, subDbId=%d, "
                       "serialNum=%" PRIu64 ")",
                       lids.get() ? lids->getDocIdLimit() : 0,
                       _subDbId, getSerialNum());
}

} // namespace proton
