# mod-transmog-collect

Collects a transmog appearance when gear is **sold to a vendor** or
**disenchanted**.

[mod-transmog](https://github.com/azerothcore/mod-transmog)'s collection
system unlocks an appearance when an item is looted, equipped, crafted,
bought or taken as a quest reward. Selling and disenchanting are not on that
list - and they are what happens to gear the player has decided not to keep,
so the appearance of a piece that was vendored without ever being worn was
lost for good.

This module records those two moments. It changes nothing else: the recording
is `Transmogrification::AddToDatabase`, the same call mod-transmog's own
hooks make, so the rules about what may be collected (armour and weapons
only; the quality and armour-type rules unless `TrackUnusableItems` is on),
the account-wide dedupe, the chat notice and the
`custom_unlocked_appearances` row are identical to an appearance collected
any other way.

## How

Two hooks the core already provides, so mod-transmog stays an unmodified
upstream clone:

| hook | fires | why it works |
| --- | --- | --- |
| `OnPlayerCanSellItem` | `HandleSellItemOpcode`, before the sale | the item is still in the bags and the vendor is in hand; the module records it and returns true, so the sale proceeds (and buyback is unaffected) |
| `OnPlayerBeforeSendLoot` | `Player::SendLoot` | disenchanting is `Spell::EffectDisEnchant` calling `SendLoot` with `LOOT_DISENCHANTING` on the item's own GUID, so the loot type identifies the case exactly and the GUID still finds the item |

With `MODULES=static` every module compiles into one library, so the call
into mod-transmog is direct and its headers are already on the include path.

## Configuration (`mod_transmog_collect.conf`)

| key | meaning |
| --- | --- |
| `TransmogCollect.Enable` | master switch |
| `TransmogCollect.OnSell` | collect when gear is sold to a vendor |
| `TransmogCollect.OnDisenchant` | collect when gear is disenchanted |

## Requirements

mod-transmog, with `Transmogrification.UseCollectionSystem = 1`.

## Licence

GNU Affero General Public License v3.0, the licence AzerothCore and its
modules use. See [LICENSE](LICENSE).
