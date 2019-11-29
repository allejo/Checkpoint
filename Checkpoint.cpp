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
const int BUILD = 2;
const std::string SUFFIX = "DEV";

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
    std::map<std::string, CheckpointZone> checkpointZones;
    std::map<std::string, std::vector<CheckpointZone*>> savedCheckpoints;
    std::map<int, std::vector<CheckpointZone*>> checkpoints;

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

    bz_registerCustomBZDBInt(bzdb_checkPointsLifetime, 60, 0, false);
    bz_registerCustomBZDBBool(bzdb_clearCheckPointsOnCap, false, 0, false);

    bz_registerCustomMapObject("CHECKPOINT", this);
}

void Checkpoint::Cleanup()
{
    Flush();

    bz_removeCustomSlashCommand("checkpoints");

    bz_removeCustomMapObject("checkpoint");

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

            if (checkpoints[data->playerID].size() == 0)
            {
                return;
            }

            CheckpointZone* zone = checkpoints[data->playerID].at(checkpoints[data->playerID].size() - 1);

            if (!zone)
            {
                return;
            }

            float pos[3];
            bz_getSpawnPointWithin(zone, pos);

            data->handled = true;
            data->pos[0] = pos[0];
            data->pos[1] = pos[1];
            data->pos[2] = pos[2] + 0.001f; // Fudge the Z-value to avoid stickiness on objects
        }
        break;

        case bz_ePlayerJoinEvent:
        {
            bz_PlayerJoinPartEventData_V1* data = (bz_PlayerJoinPartEventData_V1*)eventData;
            bz_ApiString bzid = data->record->bzID;

            // Only restore a player's last Checkpoint if they're registered
            if (data->record->verified)
            {
                unsigned long index = savedCheckpoints[bzid].size();

                if (index == 0) {
                    return;
                }

                checkpoints[data->playerID] = savedCheckpoints[bzid];
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

            for (auto &zone : checkpointZones)
            {
                auto &myCheckpoints = checkpoints[data->playerID];

                if (std::count(myCheckpoints.begin(), myCheckpoints.end(), &zone.second))
                {
                    continue;
                }

                if (zone.second.pointInZone(data->state.pos))
                {
                    bz_BasePlayerRecord *pr = bz_getPlayerByIndex(data->playerID);

                    // If a Checkpoint is intended only for a team
                    if (zone.second.team_value != eNoTeam && pr->team != zone.second.team_value)
                    {
                        continue;
                    }

                    checkpoints[data->playerID].push_back(&zone.second);

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
    if (command == "checkpoints")
    {
        auto &myCheckpoints = checkpoints[playerID];

        int max = std::min(10, (int)myCheckpoints.size());

        bz_sendTextMessagef(BZ_SERVER, playerID, "Your Checkpoints");
        bz_sendTextMessagef(BZ_SERVER, playerID, "----------------");

        for (int i = 0; i < max; ++i)
        {
            bz_sendTextMessagef(BZ_SERVER, playerID, "  - %s", myCheckpoints.at(myCheckpoints.size() - 1 - i)->name_value.c_str());
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

    if (checkpointZones.find(checkpointZone.name_value) == checkpointZones.end())
    {
        checkpointZones[checkpointZone.name_value] = checkpointZone;
    }
    else
    {
        bz_debugMessagef(0, "ERROR :: Checkpoint :: A checkpoint with the name \"%s\" already exists.", checkpointZone.name_value.c_str());
    }

    return true;
}