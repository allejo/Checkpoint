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

#include "bzfsAPI.h"
#include "plugin_utils.h"

class CheckpointZone : public bz_CustomZoneObject
{
public:
    CheckpointZone() : bz_CustomZoneObject()
    {
    }

    std::string name_value;
    bz_eTeamType team_value;
    std::string message_value;
};

class Checkpoint : public bz_Plugin, public bz_CustomSlashCommandHandler, public bz_CustomMapObjectHandler
{
public:
    virtual const char* Name();
    virtual void Init(const char* config);
    virtual void Cleanup();
    virtual void Event(bz_EventData* eventData);
    virtual bool SlashCommand(int playerID, bz_ApiString command, bz_ApiString /*message*/, bz_APIStringList *params);
    virtual bool MapObject(bz_ApiString object, bz_CustomMapObjectInfo* data);
};

BZ_PLUGIN(Checkpoint)

const char* Checkpoint::Name()
{
    return "Checkpoint";
}

void Checkpoint::Init(const char* config)
{
    Register(bz_eCaptureEvent);
    Register(bz_ePlayerJoinEvent);
    Register(bz_ePlayerUpdateEvent);

    bz_registerCustomSlashCommand("checkpoints", this);

    bz_registerCustomBZDBBool("_clearCheckPointsOnCap", false, 0, false);

    bz_registerCustomMapObject("CHECKPOINT", this);
}

void Checkpoint::Cleanup()
{
    Flush();

    bz_removeCustomSlashCommand("checkpoints");

    bz_removeCustomMapObject("checkpoint");

    bz_removeCustomBZDBVariable("_clearCheckPointsOnCap");
}

void Checkpoint::Event(bz_EventData* eventData)
{
    switch (eventData->eventType)
    {
        case bz_eCaptureEvent:
        {
            // This event is called each time a team's flag has been captured
            bz_CTFCaptureEventData_V1* data = (bz_CTFCaptureEventData_V1*)eventData;

            // Data
            // ----
            // (bz_eTeamType) teamCapped    - The team whose flag was captured.
            // (bz_eTeamType) teamCapping   - The team who did the capturing.
            // (int)          playerCapping - The player who captured the flag.
            // (float[3])     pos           - The world position(X,Y,Z) where the flag has been captured
            // (float)        rot           - The rotational orientation of the capturing player
            // (double)       eventTime     - This value is the local server time of the event.
        }
        break;

        case bz_ePlayerJoinEvent:
        {
            // This event is called each time a player joins the game
            bz_PlayerJoinPartEventData_V1* data = (bz_PlayerJoinPartEventData_V1*)eventData;

            // Data
            // ----
            // (int)                  playerID  - The player ID that is joining
            // (bz_BasePlayerRecord*) record    - The player record for the joining player
            // (double)               eventTime - Time of event.
        }
        break;

        case bz_ePlayerUpdateEvent:
        {
            // This event is called each time a player sends an update to the server
            bz_PlayerUpdateEventData_V1* data = (bz_PlayerUpdateEventData_V1*)eventData;

            // Data
            // ----
            // (int)                  playerID  - ID of the player that sent the update
            // (bz_PlayerUpdateState) state     - The original state the tank was in
            // (bz_PlayerUpdateState) lastState - The second state the tank is currently in to show there was an update
            // (double)               stateTime - The time the state was updated
            // (double)               eventTime - The current server time
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

    // @TODO Save your custom map objects to your class

    return true;
}