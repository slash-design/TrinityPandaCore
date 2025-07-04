/*
 * This file is part of the DestinyCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _LOOTMGR_H
#define _LOOTMGR_H

#include "ItemEnchantmentMgr.h"
#include "ByteBuffer.h"
#include "RefManager.h"
#include "SharedDefines.h"
#include "ConditionMgr.h"
#include "Object.h"
#include "ItemPrototype.h"

#include <map>
#include <vector>
#include <list>

enum RollType
{
    ROLL_PASS         = 0,
    ROLL_NEED         = 1,
    ROLL_GREED        = 2,
    ROLL_DISENCHANT   = 3,
    MAX_ROLL_TYPE     = 4
};

enum RollMask
{
    ROLL_FLAG_TYPE_PASS         = 0x01,
    ROLL_FLAG_TYPE_NEED         = 0x02,
    ROLL_FLAG_TYPE_GREED        = 0x04,
    ROLL_FLAG_TYPE_DISENCHANT   = 0x08,

    ROLL_ALL_TYPE_NO_DISENCHANT = 0x07,
    ROLL_ALL_TYPE_MASK          = 0x0F
};

#define MAX_LOOT_ITEMS 256

enum LootMethod
{
    FREE_FOR_ALL      = 0,
    ROUND_ROBIN       = 1,
    MASTER_LOOT       = 2,
    GROUP_LOOT        = 3,
    NEED_BEFORE_GREED = 4
};

enum PermissionTypes
{
    ALL_PERMISSION              = 0,
    GROUP_PERMISSION            = 1,
    MASTER_PERMISSION           = 2,
    ROUND_ROBIN_PERMISSION      = 3,
    OWNER_PERMISSION            = 4,
    NONE_PERMISSION             = 5
};

enum LootItemType
{
    LOOT_ITEM_TYPE_ITEM         = 0,
    LOOT_ITEM_TYPE_CURRENCY     = 1,
};

enum LootType
{
    LOOT_CORPSE                 = 1,
    LOOT_PICKPOCKETING          = 2,
    LOOT_FISHING                = 3,
    LOOT_DISENCHANTING          = 4,
                                                            // ignored always by client
    LOOT_SKINNING               = 6,
    LOOT_PROSPECTING            = 7,
    LOOT_MILLING                = 8,

    LOOT_FISHINGHOLE            = 20,                       // unsupported by client, sending LOOT_FISHING instead
    LOOT_INSIGNIA               = 21,                       // unsupported by client, sending LOOT_CORPSE instead
    LOOT_MINING_OR_HERBALISM    = 22,                       // server-side only, for guild perk Bountiful Bags, real is LOOT_SKINNING
};

// type of Loot Item in Loot View
enum LootSlotType
{
    LOOT_SLOT_TYPE_ROLL_ONGOING = 7,                        // roll is ongoing. player cannot loot.
    LOOT_SLOT_TYPE_LOCKED       = 5,                        // item is shown in red. player cannot loot.
    LOOT_SLOT_TYPE_MASTER       = 2,                        // item can only be distributed by group loot master.
    LOOT_SLOT_TYPE_ALLOW_LOOT   = 3,                        // player can loot the item.
    LOOT_SLOT_TYPE_OWNER        = 4                         // ignore binding confirmation and etc, for single player looting
};

class Player;
class LootStore;

struct LootStoreItem
{
    uint32  itemid;                                         // id of the item
    uint8   type;                                           // 0 = item, 1 = currency
    float   chance;                                         // always positive, chance to drop for both quest and non-quest items, chance to be used for refs
    int32   mincountOrRef;                                  // mincount for drop items (positive) or minus referenced TemplateleId (negative)
    uint32  lootmode;
    uint8   group       :7;
    bool    needs_quest :1;                                 // quest drop (negative ChanceOrQuestChance in DB)
    uint32  maxcount;                                       // max drop count for the item (mincountOrRef positive) or Ref multiplicator (mincountOrRef negative)
    ConditionList conditions;                               // additional loot condition

    // Constructor, converting ChanceOrQuestChance -> (chance, needs_quest)
    // displayid is filled in IsValid() which must be called after
    LootStoreItem(uint32 _itemid, uint8 _type, float _chanceOrQuestChance, uint32 _lootmode, uint8 _group, int32 _mincountOrRef, uint32 _maxcount)
        : itemid(_itemid), type(_type), chance(fabs(_chanceOrQuestChance)), mincountOrRef(_mincountOrRef), lootmode(_lootmode),
        group(_group), needs_quest(_chanceOrQuestChance < 0), maxcount(_maxcount)
         { }

    bool Roll(bool rate) const;                             // Checks if the entry takes it's chance (at loot generation)
    bool IsValid(LootStore const& store, uint32 entry) const;
                                                            // Checks correctness of values
};

typedef std::set<uint32> AllowedLooterSet;

struct WorldDropLootItem
{
private:
    struct DropChance
    {
        uint8  LevelMin;
        uint8  LevelMax;
        float  Chance;
    };

public:
    void AddDrop(uint8 levelMin, uint8 levelMax, float chance)
    {
        m_chances.push_back({ levelMin, levelMax, chance });
    }

    float GetChance(uint8 level) const
    {
        for (auto&& itr : m_chances)
            if (level >= itr.LevelMin && level <= itr.LevelMax)
                return itr.Chance;
        return 0.0f;
    }

    uint32 Item;
    uint32 Group;

private:
    std::vector<DropChance> m_chances;
};

struct LootItem
{
    uint32  itemid;
    uint8   type;                                           // 0 = item, 1 = currency
    uint32  randomSuffix;
    int32   randomPropertyId;
    ConditionList conditions;                               // additional loot condition
    AllowedLooterSet allowedGUIDs;
    uint32  count;
    bool    currency          : 1;
    bool    is_looted         : 1;
    bool    is_blocked        : 1;
    bool    freeforall        : 1;                          // free for all
    bool    is_underthreshold : 1;
    bool    is_counted        : 1;
    bool    is_world_drop     : 1;
    bool    personal          : 1;
    bool    needs_quest       : 1;                          // quest drop
    bool    follow_loot_rules : 1;
    bool    canSave;

    // Constructor, copies most fields from LootStoreItem, generates random count and random suffixes/properties
    // Should be called for non-reference LootStoreItem entries only (mincountOrRef > 0)
    explicit LootItem(LootStoreItem const& li);
    explicit LootItem(WorldDropLootItem const& item);

    // Empty constructor for creating an empty LootItem to be filled in with DB data
    LootItem() : itemid(0), type(0), randomSuffix(0), randomPropertyId(0), count(0), currency(false), is_looted(false), is_blocked(false),
        freeforall(false), is_underthreshold(false), is_counted(false), needs_quest(false), follow_loot_rules(false), canSave(true) { };

    // Basic checks for player/item compatibility - if false no chance to see the item in the loot
    bool AllowedForPlayer(Player const* player, Object* lootedObject) const;
    void AddAllowedLooter(Player const* player);
    const AllowedLooterSet & GetAllowedLooters() const { return allowedGUIDs; }

    // Write packet data
    void WriteBitDataPart(uint8 permission, ByteBuffer* buff);
    void WriteBasicDataPart(uint8 slot, ByteBuffer* buff);
};

struct QuestItem
{
    uint8   index;                                          // position in quest_items;
    bool    is_looted;

    QuestItem()
        : index(0), is_looted(false) { }

    QuestItem(uint8 _index, bool _islooted = false)
        : index(_index), is_looted(_islooted) { }
};

struct Loot;
class LootTemplate;
class Item;

typedef std::vector<QuestItem> QuestItemList;
typedef std::vector<LootItem> LootItemList;
typedef std::map<uint32, QuestItemList*> QuestItemMap;
typedef std::list<LootStoreItem*> LootStoreItemList;
typedef std::unordered_map<uint32, LootTemplate*> LootTemplateMap;

typedef std::set<uint32> LootIdSet;

class LootStore
{
    public:
        explicit LootStore(char const* name, char const* entryName, bool ratesAllowed)
            : m_name(name), m_entryName(entryName), m_ratesAllowed(ratesAllowed) { }

        virtual ~LootStore() { Clear(); }

        void Verify() const;

        uint32 LoadAndCollectLootIds(LootIdSet& ids_set);
        void CheckLootRefs(LootIdSet* ref_set = NULL) const; // check existence reference and remove it from ref_set
        void ReportUnusedIds(LootIdSet const& ids_set) const;
        void ReportNotExistedId(uint32 id) const;

        bool HaveLootFor(uint32 loot_id) const { return m_LootTemplates.find(loot_id) != m_LootTemplates.end(); }
        bool HaveQuestLootFor(uint32 loot_id) const;
        bool HaveQuestLootForPlayer(uint32 loot_id, Player* player) const;

        LootTemplate const* GetLootFor(uint32 loot_id) const;
        void ResetConditions();
        LootTemplate* GetLootForConditionFill(uint32 loot_id);

        char const* GetName() const { return m_name; }
        char const* GetEntryName() const { return m_entryName; }
        bool IsRatesAllowed() const { return m_ratesAllowed; }
    protected:
        uint32 LoadLootTable();
        void Clear();
    private:
        LootTemplateMap m_LootTemplates;
        char const* m_name;
        char const* m_entryName;
        bool m_ratesAllowed;
};

class BonusLoot;

class LootTemplate
{
    friend class BonusLoot;
    class LootGroup;                                       // A set of loot definitions for items (refs are not allowed inside)
    typedef std::vector<LootGroup*> LootGroups;

    public:
        LootTemplate() { }
        ~LootTemplate();

        // Adds an entry to the group (at loading stage)
        void AddEntry(LootStoreItem* item);
        // Rolls for every item in the template and adds the rolled items the the loot
        void Process(Loot& loot, bool rate, uint32 lootmode, uint8 groupId = 0, Player* player = NULL) const;
        void CopyConditions(const ConditionList& conditions);
        void CopyConditions(LootItem* li) const;

        // True if template includes at least 1 quest drop entry
        bool HasQuestDrop(LootTemplateMap const& store, uint8 groupId = 0) const;
        // True if template includes at least 1 quest drop for an active quest of the player
        bool HasQuestDropForPlayer(LootTemplateMap const& store, Player const* player, uint8 groupId = 0) const;

        // Checks integrity of the template
        void Verify(LootStore const& store, uint32 Id) const;
        void CheckLootRefs(LootTemplateMap const& store, LootIdSet* ref_set) const;
        bool addConditionItem(Condition* cond);
        bool isReference(uint32 id);

    private:
        // By "scripted" I mean "hardcoded", yeah
        bool ProcessScriptedLoot(Loot& loot, Player* player) const;

    private:
        LootStoreItemList Entries;                          // not grouped only
        LootGroups        Groups;                           // groups have own (optimised) processing, grouped entries go there

        // Objects of this class must never be copied, we are storing pointers in container
        LootTemplate(LootTemplate const&);
        LootTemplate& operator=(LootTemplate const&);
};

//=====================================================

class LootValidatorRef :  public Reference<Loot, LootValidatorRef>
{
    public:
        LootValidatorRef() { }
        void targetObjectDestroyLink() { }
        void sourceObjectDestroyLink() { }
};

//=====================================================

class LootValidatorRefManager : public RefManager<Loot, LootValidatorRef>
{
    public:
        typedef LinkedListHead::Iterator< LootValidatorRef > iterator;

        LootValidatorRef* getFirst() { return (LootValidatorRef*)RefManager<Loot, LootValidatorRef>::getFirst(); }
        LootValidatorRef* getLast() { return (LootValidatorRef*)RefManager<Loot, LootValidatorRef>::getLast(); }

        iterator begin() { return iterator(getFirst()); }
        iterator end() { return iterator(NULL); }
        iterator rbegin() { return iterator(getLast()); }
        iterator rend() { return iterator(NULL); }
};

//=====================================================
struct LootView;

struct Loot
{
    QuestItemMap const& GetPlayerCurrencies() const { return PlayerCurrencies; }
    QuestItemMap const& GetPlayerQuestItems() const { return PlayerQuestItems; }
    QuestItemMap const& GetPlayerFFAItems() const { return PlayerFFAItems; }
    QuestItemMap const& GetPlayerNonQuestNonFFANonCurrencyConditionalItems() const { return PlayerNonQuestNonFFANonCurrencyConditionalItems; }
    std::map<uint32, QuestItemList> const& GetPersonalItems() { return PlayerPersonalItems; }

    std::vector<LootItem> items;
    std::vector<LootItem> quest_items;
    uint32 gold;
    uint8 unlootedCount;
    uint64 roundRobinPlayer = 0;                            // GUID of the player having the Round-Robin ownership for the loot. If 0, round robin owner has released.
    LootType loot_type;                                     // required for achievement system
    uint8 maxDuplicates;                                    // Max amount of items with the same entry that can drop (default is 1; on 25 man raid mode 3)
    uint32 guildPerkChance = 0;
    float gatheringCoefficient = 0.0f;
    TypeID sourceTypeId = TYPEID_OBJECT;
    uint32 sourceEntry = 0;

    // GUIDLow of container that holds this loot (item_instance.entry)
    //  Only set for inventory items that can be right-click looted
    uint32 containerID;
    ItemTemplate const* containerItemTemplate = NULL;

    Loot(uint32 _gold = 0) : gold(_gold), unlootedCount(0), loot_type(LOOT_CORPSE), maxDuplicates(1), containerID(0) { }
    ~Loot() { clear(); }

    // For deleting items at loot removal since there is no backward interface to the Item()
    void DeleteLootItemFromContainerItemDB(uint32 itemID);
    void DeleteLootMoneyFromContainerItemDB();

    // if loot becomes invalid this reference is used to inform the listener
    void addLootValidatorRef(LootValidatorRef* pLootValidatorRef)
    {
        i_LootValidatorRefManager.insertFirst(pLootValidatorRef);
    }

    // void clear();
    void clear()
    {
        for (QuestItemMap::const_iterator itr = PlayerCurrencies.begin(); itr != PlayerCurrencies.end(); ++itr)
            delete itr->second;
        PlayerCurrencies.clear();

        for (QuestItemMap::const_iterator itr = PlayerQuestItems.begin(); itr != PlayerQuestItems.end(); ++itr)
            delete itr->second;
        PlayerQuestItems.clear();

        for (QuestItemMap::const_iterator itr = PlayerFFAItems.begin(); itr != PlayerFFAItems.end(); ++itr)
            delete itr->second;
        PlayerFFAItems.clear();

        for (QuestItemMap::const_iterator itr = PlayerNonQuestNonFFANonCurrencyConditionalItems.begin(); itr != PlayerNonQuestNonFFANonCurrencyConditionalItems.end(); ++itr)
            delete itr->second;
        PlayerNonQuestNonFFANonCurrencyConditionalItems.clear();

        PlayerPersonalItems.clear();

        PlayersLooting.clear();
        items.clear();
        quest_items.clear();
        gold = 0;
        unlootedCount = 0;
        roundRobinPlayer = 0;
        gatheringCoefficient = 0.0f;
        i_LootValidatorRefManager.clearReferences();
        guildPerkChance = 0;
        m_normalLootMode = true;
        m_mainLootGenerated = false;
    }

    bool empty() const { return items.empty() && gold == 0; }
    bool isLooted() const { return gold == 0 && unlootedCount == 0; }

    void NotifyItemRemoved(uint8 lootIndex);
    void NotifyQuestItemRemoved(uint8 questIndex);
    void NotifyMoneyRemoved();
    void AddLooter(uint64 GUID) { PlayersLooting.insert(GUID); }
    void RemoveLooter(uint64 GUID) { PlayersLooting.erase(GUID); }
    bool HasLooter(uint64 GUID) { return PlayersLooting.find(GUID) != PlayersLooting.end(); }

    void generateMoneyLoot(uint32 minAmount, uint32 maxAmount);
    bool FillLoot(Object* source, uint32 lootId, LootStore const& store, Player* lootOwner, bool personal, bool noEmptyError = false, Difficulty difficulty = REGULAR_DIFFICULTY);

    // Inserts the item into the loot (called by LootTemplate processors)
    void AddItem(LootStoreItem const& item, Player* player);
    void AddItem(LootStoreItem const& item, uint32 count);

    LootItem* LootItemInSlot(uint32 lootslot, Player* player, QuestItem** qitem = nullptr, QuestItem** ffaitem = nullptr, QuestItem** conditem = nullptr, QuestItem** currency = nullptr, QuestItem** personal = nullptr);
    uint32 GetMaxSlotInLootFor(Player* player) const;
    bool hasItemFor(Player* player) const;
    bool hasOverThresholdItem() const;

    ItemPickupSourceType GetItemPickupSourceType() const;

    Object* GetSource() const { return m_source; }

    uint64 GetGUID() const { return m_guid; }
    void SetGUID(uint64 guid) { m_guid = MAKE_NEW_GUID(GUID_LOPART(guid), 0, HIGHGUID_LOOT); }

private:
    bool CanItemBeLooted(Player* player, LootStoreItem const& item, uint32& min, uint32& max);

    private:
        void FillNotNormalLootFor(Player* player, bool presentAtLooting);
        void FillCurrencyLoot(Player* player);
        void FillFFALoot(Player* player);
        void FillQuestLoot(Player* player);
        void FillNonQuestNonFFANonCurrencyConditionalLoot(Player* player, bool presentAtLooting);

        std::set<uint64> PlayersLooting;
        QuestItemMap PlayerCurrencies;
        QuestItemMap PlayerQuestItems;
        QuestItemMap PlayerFFAItems;
        QuestItemMap PlayerNonQuestNonFFANonCurrencyConditionalItems;
        std::map<uint32, QuestItemList> PlayerPersonalItems;   // Personal loot in body, not that drops to player's bag. Used for timeless isle

        // All rolls are registered here. They need to know, when the loot is not valid anymore
        LootValidatorRefManager i_LootValidatorRefManager;
        Object* m_source = nullptr;
        uint64 m_guid = 0;
        bool m_normalLootMode = true;
        bool m_mainLootGenerated = false;
};

struct LootView
{
    Loot &loot;
    Player* viewer;
    PermissionTypes permission;

    LootView(Loot &_loot, Player* _viewer, PermissionTypes _permission = ALL_PERMISSION)
        : loot(_loot), viewer(_viewer), permission(_permission) { }

    void WriteData(ObjectGuid guid, LootType lootType, WorldPacket* data, bool isAoE);
};

struct PersonalLootTemplate
{
    std::vector<uint32> Items;
    uint32 MoneyBag;
    uint32 MoneyBagFlex;
    uint32 QuestTracker;
};

struct BonusLootTemplate
{
    enum LootSource
    {
        Creautre = 1,
        GameObject,
        Item,
    };

    uint32 Entry;
    uint32 Spell;
    uint32 Currency;
    uint32 LootIdNormal;
    uint32 LootIdHeroic;
    uint32 LootIdPersonal;
    LootSource Source;

    uint32 GetLootIdForDifficulty(Difficulty difficulty) const;
};

struct CreatureLootCurrencyTemplate
{
    uint32 Lootmode;
    uint32 Currency;
    uint32 Count;
};

typedef std::vector<CreatureLootCurrencyTemplate> CreatureLootCurrencySet;

class PersonalLoot
{
public:
    PersonalLoot(uint32 lootId);

    void Reward(Player* player);

    operator bool() const
    {
        return m_loot;
    }

    typedef std::vector<uint32>::const_iterator iterator;
    iterator begin() const { return m_loot ? m_loot->Items.begin() : iterator(); }
    iterator end() const { return m_loot ? m_loot->Items.end() : iterator(); }

private:
    PersonalLootTemplate const* m_loot = nullptr;
};

class BonusLoot
{
public:
    BonusLoot(uint32 lootId, Difficulty difficulty);
    BonusLoot(BonusLootTemplate const* lootTemplate, Difficulty difficulty);
    void Reward(Player* player);

    operator bool() const
    {
        return !m_items.empty() || m_currency;
    }

    typedef std::list<ItemTemplate const*>::const_iterator iterator;
    iterator begin() const { return m_items.begin(); }
    iterator end() const { return m_items.end(); }

private:
    void Fill(BonusLootTemplate const* lootTemplate, Difficulty difficulty);
    void Fill(LootTemplate const* lootTemplate, bool shared = false);
    void AddItem(LootStoreItem const* lootItem, bool shared);
private:
    uint32 m_difficultyMask = 0;
    std::list<ItemTemplate const*> m_items;
    std::list<ItemTemplate const*> m_sharedItems;
    LootStoreItem const* m_currency = nullptr;
    BonusLootTemplate const* m_lootTemplate;
    bool m_test = false;
};

class AELootResult
{
public:
    struct ResultValue
    {
        ::Item* Item;
        uint8 Count;
        LootType Type;
    };

    typedef std::vector<ResultValue> OrderedStorage;

    void Add(Item* item, uint8 count, LootType lootType);

    OrderedStorage::const_iterator begin() const { return m_byOrder.begin(); }
    OrderedStorage::const_iterator end() const { return m_byOrder.end(); }

    OrderedStorage m_byOrder;
    std::unordered_map<Item*, OrderedStorage::size_type> m_byItem;
};

enum LegendaryQuestLootType
{
    None                    = 0x00,
    MoguSeals               = 0x01,
    SecretsOfEmpire         = 0x02,
    TitanRunestone          = 0x04,
    ChimeraOfFear           = 0x08,
    HeartOfTheThunderKing   = 0x10,
    Max                     = 0x20,
};

class LootMgr
{
public:
    static LootMgr* instance()
    {
        static LootMgr _instance;
        return &_instance;
    }

    CreatureLootCurrencySet const* GetCreatureLootCurrency(uint32 creature) const
    {
        auto it = m_lootCurrncy.find(creature);
        return it != m_lootCurrncy.end() ? &it->second : nullptr;
    }

    PersonalLootTemplate const* GetPersonalLoot(uint32 entry) const
    {
        auto it = m_personalLoot.find(entry);
        return it != m_personalLoot.end() ? &it->second : nullptr;
    }

    BonusLootTemplate const* GetBonusLoot(uint32 entry) const
    {
        auto it = m_bonusLoot.find(entry);
        return it != m_bonusLoot.end() ? &it->second : nullptr;
    }

    BonusLootTemplate const* GetBonusLootForSpell(uint32 spell) const
    {
        for (auto&& it : m_bonusLoot)
            if (it.second.Spell == spell)
                return &it.second;
        return nullptr;
    }

    std::map<uint32, WorldDropLootItem> const* GetWorldDrop(Creature* creature) const;

    bool IsRewardOnKill(uint32 entry);

    void RewardLegendaryQuestLoot(Player* player, uint32 entry);

    void LoadFromDB();

    void LoadCreatureLootCurrency();
    void LoadPersonalLoot();
    void LoadBonusLoot();
    void LoadWorldDrop();

private:
    std::unordered_map<uint32, CreatureLootCurrencySet> m_lootCurrncy;
    std::unordered_map<uint32, PersonalLootTemplate>    m_personalLoot;
    std::unordered_map<uint32, BonusLootTemplate>       m_bonusLoot;
    std::map<uint32, LegendaryQuestLootType> m_legendaryQuestLoot;
    std::map<uint32, WorldDropLootItem> m_worldDropLoot[MAX_EXPANSIONS];
};

#define sLootMgr LootMgr::instance()

extern LootStore LootTemplates_Creature;
extern LootStore LootTemplates_Fishing;
extern LootStore LootTemplates_Gameobject;
extern LootStore LootTemplates_Item;
extern LootStore LootTemplates_Mail;
extern LootStore LootTemplates_Milling;
extern LootStore LootTemplates_Pickpocketing;
extern LootStore LootTemplates_Reference;
extern LootStore LootTemplates_Skinning;
extern LootStore LootTemplates_Disenchant;
extern LootStore LootTemplates_Prospecting;
extern LootStore LootTemplates_Spell;

void LoadLootTemplates_Creature();
void LoadLootTemplates_Fishing();
void LoadLootTemplates_Gameobject();
void LoadLootTemplates_Item();
void LoadLootTemplates_Mail();
void LoadLootTemplates_Milling();
void LoadLootTemplates_Pickpocketing();
void LoadLootTemplates_Skinning();
void LoadLootTemplates_Disenchant();
void LoadLootTemplates_Prospecting();

void LoadLootTemplates_Spell();
void LoadLootTemplates_Reference();

inline void LoadLootTables()
{
    LoadLootTemplates_Creature();
    LoadLootTemplates_Fishing();
    LoadLootTemplates_Gameobject();
    LoadLootTemplates_Item();
    LoadLootTemplates_Mail();
    LoadLootTemplates_Milling();
    LoadLootTemplates_Pickpocketing();
    LoadLootTemplates_Skinning();
    LoadLootTemplates_Disenchant();
    LoadLootTemplates_Prospecting();
    LoadLootTemplates_Spell();

    LoadLootTemplates_Reference();
}

#endif
