/*
 * Copyright (C) 2019 Vladimir "allejo" Jimenez
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <algorithm>
#include <map>

#include "bzfsAPI.h"

// Define plug-in name
const std::string PLUGIN_NAME = "Checkpoint";

// Define plug-in version numbering
const int MAJOR = 1;
const int MINOR = 0;
const int REV = 0;
const int BUILD = 8;
const std::string SUFFIX = "STABLE";

// Define build settings
const int VERBOSITY_LEVEL = 4;

class CheckpointZone : public bz_CustomZoneObject
{
public:
    CheckpointZone() : bz_CustomZoneObject()
    {
    }

    std::string name_value;
    std::string message_value;
    bz_eTeamType team_value = eNoTeam;
};

class Checkpoint : public bz_Plugin, public bz_CustomSlashCommandHandler, public bz_CustomMapObjectHandler
{
public:
    const char* Name();
    void Init(const char* config);
    void Cleanup();
    void Event(bz_EventData* eventData);
    bool SlashCommand(int playerID, bz_ApiString command, bz_ApiString /*message*/, bz_APIStringList *params);
    bool MapObject(bz_ApiString object, bz_CustomMapObjectInfo* data);

private:
    /// The registered checkpoints this plug-in is keeping track of
    std::map<std::string, CheckpointZone> registeredCheckpoints;

    struct CheckpointRecord
    {
        std::string bzID = "";

        /// The current checkpoint a player is assigned to spawn at
        CheckpointZone* currentCheckpoint;

        /// Checkpoints this player has reached
        std::vector<CheckpointZone*> checkpoints;

        /// The orientation the player was facing when they first landed
        float azimuth = 0;

        /// The saved position within this checkpoint
        std::map<CheckpointZone*, bz_APIFloatList*> savedPositions;
    };

    /// A map to save checkpoint records between player joins indexed by BZID
    std::map<std::string, CheckpointRecord> savedCheckpoints;

    /// A map to store checkpoint records during a player's connected session indexed by playerID
    std::map<int, CheckpointRecord> checkpoints;

    const char* bzdb_checkPointsLifetime = "_checkPointsLifetime";
    const char* bzdb_clearCheckPointsOnCap = "_clearCheckPointsOnCap";
};

BZ_PLUGIN(Checkpoint)

const char* Checkpoint::Name()
{
    static const char *pluginBuild;

    if (!pluginBuild)
    {
        pluginBuild = bz_format("%s %d.%d.%d (%d)", PLUGIN_NAME.c_str(), MAJOR, MINOR, REV, BUILD);

        if (!SUFFIX.empty())
        {
            pluginBuild = bz_format("%s - %s", pluginBuild, SUFFIX.c_str());
        }
    }

    return pluginBuild;
}

void Checkpoint::Init(const char* /*config*/)
{
    Register(bz_eCaptureEvent);
    Register(bz_eGetPlayerSpawnPosEvent);
    Register(bz_ePlayerJoinEvent);
    Register(bz_ePlayerPartEvent);
    Register(bz_ePlayerUpdateEvent);

    bz_registerCustomSlashCommand("checkpoints", this);
    bz_registerCustomSlashCommand("cp", this);

    bz_registerCustomBZDBInt(bzdb_checkPointsLifetime, 5, 0, false);
    bz_registerCustomBZDBBool(bzdb_clearCheckPointsOnCap, false, 0, false);

    bz_registerCustomMapObject("CHECKPOINT", this);
}

void Checkpoint::Cleanup()
{
    for (auto &record : savedCheckpoints)
    {
        for (auto &save : record.second.savedPositions)
        {
            bz_deleteFloatList(save.second);
        }
    }

    Flush();

    bz_removeCustomSlashCommand("checkpoints");
    bz_removeCustomSlashCommand("cp");

    bz_removeCustomMapObject("CHECKPOINT");

    bz_removeCustomBZDBVariable(bzdb_checkPointsLifetime);
    bz_removeCustomBZDBVariable(bzdb_clearCheckPointsOnCap);
}

void Checkpoint::Event(bz_EventData* eventData)
{
    switch (eventData->eventType)
    {
        case bz_eCaptureEvent:
        {
            if (bz_getBZDBBool(bzdb_clearCheckPointsOnCap))
            {
                savedCheckpoints.clear();
                checkpoints.clear();
            }
        }
        break;

        case bz_eGetPlayerSpawnPosEvent:
        {
            bz_GetPlayerSpawnPosEventData_V1* data = (bz_GetPlayerSpawnPosEventData_V1*)eventData;
            CheckpointRecord* record = &checkpoints[data->playerID];

            // If the record for this player doesn't exist or they have not reached any checkpoints, don't try setting a
            // spawn position for them
            if (!record || record->checkpoints.size() == 0 || !record->currentCheckpoint)
            {
                return;
            }

            CheckpointZone* zone = record->currentCheckpoint;
            float pos[3];

            // If there's an explicitly saved position in this checkpoint, use it. Otherwise, pick a random spawn
            // position within the zone each time.
            if (record->savedPositions[zone]->size() > 0)
            {
                pos[0] = record->savedPositions[zone]->get(0);
                pos[1] = record->savedPositions[zone]->get(1);
                pos[2] = record->savedPositions[zone]->get(2);
            }
            else
            {
                bz_getSpawnPointWithin(zone, pos);
            }

            data->handled = true;
            data->pos[0] = pos[0];
            data->pos[1] = pos[1];
            data->pos[2] = pos[2] + 0.001f; // Fudge the Z-value to avoid stickiness on objects
            data->rot = record->azimuth; // Always spawn them facing the same direction as the first time
        }
        break;

        case bz_ePlayerJoinEvent:
        {
            bz_PlayerJoinPartEventData_V1* data = (bz_PlayerJoinPartEventData_V1*)eventData;
            bz_ApiString bzid = data->record->bzID;

            // Only restore a player's last Checkpoint if they're registered
            if (data->record->verified)
            {
                if (savedCheckpoints.find(bzid) != savedCheckpoints.end())
                {
                    checkpoints[data->playerID] = savedCheckpoints[bzid];
                }
            }

            if (checkpoints.find(data->playerID) == checkpoints.end())
            {
                checkpoints[data->playerID] = CheckpointRecord();
                checkpoints[data->playerID].bzID = data->record->bzID;
            }
        }
        break;

        case bz_ePlayerPartEvent:
        {
            bz_PlayerJoinPartEventData_V1* data = (bz_PlayerJoinPartEventData_V1*)eventData;

            if (data->record->verified)
            {
                savedCheckpoints[data->record->bzID] = checkpoints[data->playerID];
            }

            checkpoints.erase(data->playerID);
        }
        break;

        case bz_ePlayerUpdateEvent:
        {
            bz_PlayerUpdateEventData_V1* data = (bz_PlayerUpdateEventData_V1*)eventData;

            // If they're falling, don't bother doing anything
            if (data->state.falling)
            {
                return;
            }

            for (auto &zone : registeredCheckpoints)
            {
                auto &myCheckpoints = checkpoints[data->playerID].checkpoints;

                if (std::count(myCheckpoints.begin(), myCheckpoints.end(), &zone.second))
                {
                    continue;
                }

                // They're now inside a checkpoint they're never been in
                if (zone.second.pointInZone(data->state.pos))
                {
                    bz_BasePlayerRecord *pr = bz_getPlayerByIndex(data->playerID);

                    // If a checkpoint is intended only for a specific team color
                    if (zone.second.team_value != eNoTeam && pr->team != zone.second.team_value)
                    {
                        continue;
                    }

                    checkpoints[data->playerID].checkpoints.push_back(&zone.second);
                    checkpoints[data->playerID].currentCheckpoint = &zone.second;
                    checkpoints[data->playerID].azimuth = data->state.rotation;
                    checkpoints[data->playerID].savedPositions[&zone.second] = bz_newFloatList();

                    if (!zone.second.message_value.empty())
                    {
                        bz_sendTextMessagef(BZ_SERVER, data->playerID, zone.second.message_value.c_str());
                    }

                    bz_freePlayerRecord(pr);
                    break;
                }
            }
        }
        break;

        default:
            break;
    }
}

bool Checkpoint::SlashCommand(int playerID, bz_ApiString command, bz_ApiString /*message*/, bz_APIStringList *params)
{
    if (command == "checkpoints" || command == "cp")
    {
        if (params->size() == 0)
        {
            bz_sendTextMessagef(BZ_SERVER, playerID, "Syntax: /%s <list|save|swap>", command.c_str());
            return true;
        }

        auto &playerCPs = checkpoints[playerID].checkpoints;

        if (params->get(0) == "list")
        {
            // Only list a maximum of the 10 most recent checkpoints
            int max = std::min(10, (int)playerCPs.size());

            bz_sendTextMessagef(BZ_SERVER, playerID, "Your Checkpoints");
            bz_sendTextMessagef(BZ_SERVER, playerID, "----------------");

            for (int i = 0; i < max; ++i)
            {
                // The more recent checkpoints are always appended
                CheckpointZone* cp = playerCPs.at(playerCPs.size() - 1 - i);
                bool isSelected = (cp == checkpoints[playerID].currentCheckpoint);

                bz_sendTextMessagef(BZ_SERVER, playerID, "  %s %s", isSelected ? "*" : "-", cp->name_value.c_str());
            }
        }
        else if (params->get(0) == "save")
        {
            bz_BasePlayerRecord* pr = bz_getPlayerByIndex(playerID);
            bool locationSaved = false;

            // Get the player's current position to make sure they're inside a checkpoint
            float currPos[3];
            currPos[0] = pr->lastKnownState.pos[0];
            currPos[1] = pr->lastKnownState.pos[1];
            currPos[2] = pr->lastKnownState.pos[2];

            for (auto &zone : playerCPs)
            {
                if (zone->pointInZone(currPos))
                {
                    checkpoints[playerID].savedPositions[zone]->clear();
                    checkpoints[playerID].savedPositions[zone]->push_back(currPos[0]);
                    checkpoints[playerID].savedPositions[zone]->push_back(currPos[1]);
                    checkpoints[playerID].savedPositions[zone]->push_back(currPos[2]);
                    checkpoints[playerID].azimuth = pr->lastKnownState.rotation;

                    bz_sendTextMessagef(BZ_SERVER, playerID, "You have changed your default spawn location:");
                    bz_sendTextMessagef(BZ_SERVER, playerID, "  Next spawn will be be at this position.");

                    locationSaved = true;

                    break;
                }
            }

            if (!locationSaved)
            {
                bz_sendTextMessagef(BZ_SERVER, playerID, "You are not currently inside of a checkpoint.");
            }

            bz_freePlayerRecord(pr);
        }
        else if (params->get(0) == "swap")
        {
            if (params->size() != 2)
            {
                bz_sendTextMessagef(BZ_SERVER, playerID, "Syntax: /%s swap \"<checkpoint name>\"", command.c_str());
                return true;
            }

            bz_ApiString targetCheckpoint = bz_tolower(params->get(1).c_str());
            bool locationSaved = false;

            for (auto &zone : playerCPs)
            {
                bz_ApiString zoneName = bz_tolower(zone->name_value.c_str());

                if (targetCheckpoint == zoneName)
                {
                    checkpoints[playerID].currentCheckpoint = zone;
                    locationSaved = true;

                    bz_sendTextMessagef(BZ_SERVER, playerID, "You have changed your default checkpoint:");
                    bz_sendTextMessagef(BZ_SERVER, playerID, "  Next spawn will be at: %s", zone->name_value.c_str());

                    break;
                }
            }

            if (!locationSaved)
            {
                bz_sendTextMessagef(BZ_SERVER, playerID, "The checkpoint \"%s\" does not exist or you have not reached it yet.", targetCheckpoint.c_str());
            }
        }

        return true;
    }

    return false;
}

bool Checkpoint::MapObject(bz_ApiString object, bz_CustomMapObjectInfo* data)
{
    // Note, this value will be in uppercase
    if (!data || object != "CHECKPOINT")
    {
        return false;
    }

    CheckpointZone checkpointZone;
    checkpointZone.handleDefaultOptions(data);

    for (unsigned int i = 0; i < data->data.size(); i++)
    {
        std::string line = data->data.get(i);

        bz_APIStringList nubs;
        nubs.tokenize(line.c_str(), " ", 0, true);

        if (nubs.size() > 0)
        {
            std::string key = bz_toupper(nubs.get(0).c_str());

            if (key == "NAME")
            {
                checkpointZone.name_value = nubs.get(1).c_str();
            }
            else if (key == "TEAM")
            {
                checkpointZone.team_value = (bz_eTeamType)atoi(nubs.get(1).c_str());
            }
            else if (key == "MESSAGE")
            {
                checkpointZone.message_value = nubs.get(1).c_str();
            }
        }
    }

    if (registeredCheckpoints.find(checkpointZone.name_value) == registeredCheckpoints.end())
    {
        registeredCheckpoints[checkpointZone.name_value] = checkpointZone;
    }
    else
    {
        bz_debugMessagef(0, "ERROR :: Checkpoint :: A checkpoint with the name \"%s\" already exists.", checkpointZone.name_value.c_str());
    }

    return true;
}
