/*
 * mod-transmog-collect - collect transmog appearances from gear that is sold
 * to a vendor or disenchanted.
 *
 * mod-transmog's Legion-style collection unlocks an appearance when an item is
 * looted, equipped, crafted, bought from a vendor or taken as a quest reward
 * (see the hooks in its transmog_scripts.cpp). Selling and disenchanting are
 * not on that list, and they are exactly what happens to the gear a player has
 * decided not to keep - so the appearance of a piece that was never equipped,
 * only vendored, was lost.
 *
 * Two hooks the core already provides are enough, and neither needs a change
 * to mod-transmog:
 *
 *   OnPlayerCanSellItem     fires in HandleSellItemOpcode before the sale is
 *                           carried out, with the item still in the bags and
 *                           the vendor in hand. The item is recorded and true
 *                           is returned, so the sale proceeds as normal.
 *
 *   OnPlayerBeforeSendLoot  fires in Player::SendLoot. Disenchanting is
 *                           Spell::EffectDisEnchant calling SendLoot with
 *                           LOOT_DISENCHANTING on the item's own GUID, so the
 *                           loot type identifies the case exactly and the GUID
 *                           finds the item while it still exists.
 *
 * The recording itself is Transmogrification::AddToDatabase, the same call
 * mod-transmog's own hooks make, so every rule about what may be collected
 * (armour and weapons only, the quality and armour-type rules unless
 * TrackUnusableItems is on), the account-wide dedupe, the chat notice and the
 * custom_unlocked_appearances row behave as they do for any other source.
 *
 * With MODULES=static every module is compiled into one library, so calling
 * into mod-transmog is a direct call and its headers are already on the
 * include path.
 */

#include "Config.h"
#include "Group.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "LootMgr.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "Transmogrification.h"

namespace
{
    struct TransmogCollectConfig
    {
        bool Enable       = true;
        bool OnSell       = true;
        bool OnDisenchant = true;
    };

    TransmogCollectConfig cfg;

    void LoadConfig()
    {
        cfg.Enable       = sConfigMgr->GetOption<bool>("TransmogCollect.Enable", true);
        cfg.OnSell       = sConfigMgr->GetOption<bool>("TransmogCollect.OnSell", true);
        cfg.OnDisenchant = sConfigMgr->GetOption<bool>("TransmogCollect.OnDisenchant", true);
    }

    // Hand the template to mod-transmog, which decides whether it is
    // collectable and whether the account already has it.
    void CollectEntry(Player* player, uint32 itemEntry)
    {
        if (!player || !itemEntry)
            return;

        if (!sTransmogrification->GetUseCollectionSystem())
            return;

        if (ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemEntry))
            sTransmogrification->AddToDatabase(player, proto);
    }

    void Collect(Player* player, Item const* item)
    {
        if (item)
            CollectEntry(player, item->GetEntry());
    }
}

class TransmogCollect_WorldScript : public WorldScript
{
public:
    TransmogCollect_WorldScript() : WorldScript("TransmogCollect_WorldScript",
        { WORLDHOOK_ON_AFTER_CONFIG_LOAD }) { }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        LoadConfig();
    }
};

class TransmogCollect_PlayerScript : public PlayerScript
{
public:
    TransmogCollect_PlayerScript() : PlayerScript("TransmogCollect_PlayerScript",
        { PLAYERHOOK_CAN_SELL_ITEM, PLAYERHOOK_ON_BEFORE_SEND_LOOT,
          PLAYERHOOK_ON_GROUP_ROLL_REWARD_ITEM }) { }

    // Returning true here only means "the sale may go ahead", which is what
    // every other implementation of this hook returns; the collection is the
    // side effect.
    bool OnPlayerCanSellItem(Player* player, Item* item, Creature* /*creature*/) override
    {
        if (cfg.Enable && cfg.OnSell)
            Collect(player, item);

        return true;
    }

    void OnPlayerBeforeSendLoot(Player* player, ObjectGuid lootGuid, Loot* loot) override
    {
        if (!cfg.Enable || !cfg.OnDisenchant || !loot)
            return;

        if (loot->loot_type != LOOT_DISENCHANTING)
            return;

        // For a disenchant the loot GUID is the item being destroyed.
        if (!lootGuid.IsItem())
            return;

        Collect(player, player->GetItemByGuid(lootGuid));
    }

    // Winning a *roll* to disenchant never goes through SendLoot: Group.cpp
    // marks the item looted in the corpse and hands over only the materials,
    // so the item never exists in anybody's bags and the hook above cannot
    // see it. This is the same event reported through the roll hook, where
    // the item pointer is null for a disenchant and roll->itemid identifies
    // what was destroyed.
    //
    // Need and greed wins are collected here too. They arrive as a real item
    // and would eventually be collected on equip - or on being vendored, now
    // - but an appearance the player won outright should not wait for that.
    void OnPlayerGroupRollRewardItem(Player* player, Item* item, uint32 /*count*/,
                                    RollVote voteType, Roll* roll) override
    {
        if (!cfg.Enable)
            return;

        if (voteType == DISENCHANT)
        {
            if (cfg.OnDisenchant && roll)
                CollectEntry(player, roll->itemid);

            return;
        }

        if (item)
            Collect(player, item);
        else if (roll)
            CollectEntry(player, roll->itemid);
    }
};

void AddTransmogCollectScripts()
{
    new TransmogCollect_WorldScript();
    new TransmogCollect_PlayerScript();
}
