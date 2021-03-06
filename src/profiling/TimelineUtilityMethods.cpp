//
// Copyright © 2019 Arm Ltd. All rights reserved.
// SPDX-License-Identifier: MIT
//

#include "TimelineUtilityMethods.hpp"
#include "ProfilingService.hpp"
#include "LabelsAndEventClasses.hpp"

namespace armnn
{

namespace profiling
{

void TimelineUtilityMethods::SendWellKnownLabelsAndEventClasses()
{
    // Send the "name" label, this call throws in case of error
    m_SendTimelinePacket.SendTimelineLabelBinaryPacket(LabelsAndEventClasses::NAME_GUID,
                                                       LabelsAndEventClasses::NAME_LABEL);

    // Send the "type" label, this call throws in case of error
    m_SendTimelinePacket.SendTimelineLabelBinaryPacket(LabelsAndEventClasses::TYPE_GUID,
                                                       LabelsAndEventClasses::TYPE_LABEL);

    // Send the "index" label, this call throws in case of error
    m_SendTimelinePacket.SendTimelineLabelBinaryPacket(LabelsAndEventClasses::INDEX_GUID,
                                                       LabelsAndEventClasses::INDEX_LABEL);

    // Send the "start of life" event class, this call throws in case of error
    m_SendTimelinePacket.SendTimelineEventClassBinaryPacket(LabelsAndEventClasses::ARMNN_PROFILING_SOL_EVENT_CLASS);

    // Send the "end of life" event class, this call throws in case of error
    m_SendTimelinePacket.SendTimelineEventClassBinaryPacket(LabelsAndEventClasses::ARMNN_PROFILING_EOL_EVENT_CLASS);
}

ProfilingDynamicGuid TimelineUtilityMethods::CreateNamedTypedEntity(const std::string& name, const std::string& type)
{
    // Check that the entity name is valid
    if (name.empty())
    {
        throw InvalidArgumentException("Invalid entity name, the entity name cannot be empty");
    }

    // Check that the entity type is valid
    if (type.empty())
    {
        throw InvalidArgumentException("Invalid entity type, the entity type cannot be empty");
    }

    // Generate dynamic GUID of the entity
    ProfilingDynamicGuid entityGuid = ProfilingService::Instance().NextGuid();

    CreateNamedTypedEntity(entityGuid, name, type);

    return entityGuid;
}

void TimelineUtilityMethods::CreateNamedTypedEntity(ProfilingDynamicGuid entityGuid,
                                                    const std::string& name,
                                                    const std::string& type)
{
    // Check that the entity name is valid
    if (name.empty())
    {
        throw InvalidArgumentException("Invalid entity name, the entity name cannot be empty");
    }

    // Check that the entity type is valid
    if (type.empty())
    {
        throw InvalidArgumentException("Invalid entity type, the entity type cannot be empty");
    }

    // Send Entity Binary Packet of the entity to the external profiling service
    m_SendTimelinePacket.SendTimelineEntityBinaryPacket(entityGuid);

    // Create name entity and send the relationship of the entity with the given name
    NameEntity(entityGuid, name);

    // Create type entity and send the relationship of the entity with the given type
    TypeEntity(entityGuid, type);
}

ProfilingStaticGuid TimelineUtilityMethods::DeclareLabel(const std::string& labelName)
{
    // Check that the label name is valid
    if (labelName.empty())
    {
        // The label name is invalid
        throw InvalidArgumentException("Invalid label name, the label name cannot be empty");
    }

    // Generate a static GUID for the given label name
    ProfilingStaticGuid labelGuid = ProfilingService::Instance().GenerateStaticId(labelName);

    // Send the new label to the external profiling service, this call throws in case of error
    m_SendTimelinePacket.SendTimelineLabelBinaryPacket(labelGuid, labelName);

    return labelGuid;
}

void TimelineUtilityMethods::CreateTypedLabel(ProfilingGuid entityGuid,
                                              const std::string& entityName,
                                              ProfilingStaticGuid labelTypeGuid)
{
    // Check that the entity name is valid
    if (entityName.empty())
    {
        // The entity name is invalid
        throw InvalidArgumentException("Invalid entity name, the entity name cannot be empty");
    }

    // Declare a label with the entity's name, this call throws in case of error
    ProfilingStaticGuid labelGuid = DeclareLabel(entityName);

    // Generate a GUID for the label relationship
    ProfilingDynamicGuid relationshipGuid = ProfilingService::Instance().NextGuid();

    // Send the new label link to the external profiling service, this call throws in case of error
    m_SendTimelinePacket.SendTimelineRelationshipBinaryPacket(ProfilingRelationshipType::LabelLink,
                                                              relationshipGuid,
                                                              entityGuid,
                                                              labelGuid);

    // Generate a GUID for the label relationship
    ProfilingDynamicGuid relationshipLabelGuid = ProfilingService::Instance().NextGuid();

    // Send the new label link to the external profiling service, this call throws in case of error
    m_SendTimelinePacket.SendTimelineRelationshipBinaryPacket(ProfilingRelationshipType::LabelLink,
                                                              relationshipLabelGuid,
                                                              relationshipGuid,
                                                              labelTypeGuid);
}

void TimelineUtilityMethods::NameEntity(ProfilingGuid entityGuid, const std::string& name)
{
    CreateTypedLabel(entityGuid, name, LabelsAndEventClasses::NAME_GUID);
}

void TimelineUtilityMethods::TypeEntity(ProfilingGuid entityGuid, const std::string& type)
{
    CreateTypedLabel(entityGuid, type, LabelsAndEventClasses::TYPE_GUID);
}

ProfilingDynamicGuid TimelineUtilityMethods::CreateNamedTypedChildEntity(ProfilingGuid parentEntityGuid,
                                                                         const std::string& entityName,
                                                                         const std::string& entityType)
{
    // Check that the entity name is valid
    if (entityName.empty())
    {
        // The entity name is invalid
        throw InvalidArgumentException("Invalid entity name, the entity name cannot be empty");
    }

    // Check that the entity type is valid
    if (entityType.empty())
    {
        // The entity type is invalid
        throw InvalidArgumentException("Invalid entity type, the entity type cannot be empty");
    }

    // Create a named type entity from the given name and type, this call throws in case of error
    ProfilingDynamicGuid childEntityGuid = CreateNamedTypedEntity(entityName, entityType);

    // Generate a GUID for the retention link relationship
    ProfilingDynamicGuid retentionLinkGuid = ProfilingService::Instance().NextGuid();

    // Send the new retention link to the external profiling service, this call throws in case of error
    m_SendTimelinePacket.SendTimelineRelationshipBinaryPacket(ProfilingRelationshipType::RetentionLink,
                                                              retentionLinkGuid,
                                                              parentEntityGuid,
                                                              childEntityGuid);

    return childEntityGuid;
}

void TimelineUtilityMethods::CreateNamedTypedChildEntity(ProfilingDynamicGuid childEntityGuid,
                                                         ProfilingGuid parentEntityGuid,
                                                         const std::string& entityName,
                                                         const std::string& entityType)
{
    // Check that the entity name is valid
    if (entityName.empty())
    {
        // The entity name is invalid
        throw InvalidArgumentException("Invalid entity name, the entity name cannot be empty");
    }

    // Check that the entity type is valid
    if (entityType.empty())
    {
        // The entity type is invalid
        throw InvalidArgumentException("Invalid entity type, the entity type cannot be empty");
    }

    // Create a named type entity from the given guid, name and type, this call throws in case of error
    CreateNamedTypedEntity(childEntityGuid, entityName, entityType);

    // Generate a GUID for the retention link relationship
    ProfilingDynamicGuid retentionLinkGuid = ProfilingService::Instance().NextGuid();

    // Send the new retention link to the external profiling service, this call throws in case of error
    m_SendTimelinePacket.SendTimelineRelationshipBinaryPacket(ProfilingRelationshipType::RetentionLink,
                                                              retentionLinkGuid,
                                                              parentEntityGuid,
                                                              childEntityGuid);
}

ProfilingDynamicGuid TimelineUtilityMethods::RecordEvent(ProfilingGuid entityGuid, ProfilingStaticGuid eventClassGuid)
{
    // Take a timestamp
    uint64_t timestamp = GetTimestamp();

    // Get the thread id
    std::thread::id threadId = std::this_thread::get_id();

    // Generate a GUID for the event
    ProfilingDynamicGuid eventGuid = ProfilingService::Instance().NextGuid();

    // Send the new timeline event to the external profiling service, this call throws in case of error
    m_SendTimelinePacket.SendTimelineEventBinaryPacket(timestamp, threadId, eventGuid);

    // Generate a GUID for the execution link
    ProfilingDynamicGuid executionLinkId = ProfilingService::Instance().NextGuid();

    // Send the new execution link to the external profiling service, this call throws in case of error
    m_SendTimelinePacket.SendTimelineRelationshipBinaryPacket(ProfilingRelationshipType::ExecutionLink,
                                                              executionLinkId,
                                                              entityGuid,
                                                              eventGuid);

    // Generate a GUID for the data relationship link
    ProfilingDynamicGuid eventClassLinkId = ProfilingService::Instance().NextGuid();

    // Send the new data relationship link to the external profiling service, this call throws in case of error
    m_SendTimelinePacket.SendTimelineRelationshipBinaryPacket(ProfilingRelationshipType::DataLink,
                                                              eventClassLinkId,
                                                              entityGuid,
                                                              eventClassGuid);

    return eventGuid;
}

} // namespace profiling

} // namespace armnn
