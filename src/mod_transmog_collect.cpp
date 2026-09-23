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
#include "Item.h"
#include "ItemTemplate.h"
#include "LootMgr.h"
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

    // Hand the item to mod-transmog, which decides whether it is collectable
    // and whether the account already has it.
    void Collect(Player* player, Item const* item)
    {
        if (!player || !item)
            return;

        if (!sTransmogrification->GetUseCollectionSystem())
            return;

        if (ItemTemplate const* proto = item->GetTemplate())
            sTransmogrification->AddToDatabase(player, proto);
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
        { PLAYERHOOK_CAN_SELL_ITEM, PLAYERHOOK_ON_BEFORE_SEND_LOOT }) { }

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
};

void AddTransmogCollectScripts()
{
    new TransmogCollect_WorldScript();
    new TransmogCollect_PlayerScript();
}
