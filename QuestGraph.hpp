//////////////////////////////////////////////////////////////////////////
// Copyright (C) 2002 - 2015 EleFun Games								//
//////////////////////////////////////////////////////////////////////////
// File name: QuestGraph.hpp											//
//////////////////////////////////////////////////////////////////////////

#pragma once

#include "QuestEngine/Prerequisites.hpp"

namespace QuestEngine
{
    // Typedefs for quick access to quests on exact location.
	typedef std::deque<LocationNode*>	LocationDeque; 
	typedef std::map<string, QuestList> QuestsByLocationsMap;

	class QuestGraph 
	{
	public:
		QuestGraph (SceneVariableManager *_variable_manager, const char * _quests_file_name, const char * _alternative_quests_file_name = "");
		virtual ~QuestGraph();
        
		void							UpdateConditionQuests();
		void							Load(const char * _file_name, const char * _alternative_quests_file_name = "");
		
		QuestItem *						GetQuestItem(const string &_name);

		// Returns the last quest in chain of which _dst_task depends
		QuestItem *						GetLastRequiredQuest( const string &_dst_task, QuestChain &_chain );

        // Concrete quest types gettres.
		CollectInventoryItemQuest *		GetCollectInventoryItemQuest(const string &_inventory_item);
		ApplyInventoryItemQuest*		GetApplyInventoryItemQuest(const string &_inventory_item_name, const string &_drop_zone_name);
		ClickQuest *					GetClickQuest(const string &_layer_name, const string &_scene_name );
		const QuestList *				GetAllQuestsOnLocation(const string &_location_name);

		bool							IsAnyAvailableQuestOnLocation(const string &_location_name);
		void                            UpdateQuestLineProgress();
		uint32							GetQuestLineProgress() const;
		const string &					GetFinalQuestName() const;
        
        // Make all quests unchecked in quest graph.
		void							ResetCheckedState();
        
    private:
		QuestItem *						SearchQuest(QuestItem *_dst_quest, QuestChain &_chain);
        QuestItem *						AddQuestItem(XMLNodeA *_xml_node);
        
	private:
		XMLNodeA							*xml_node;

		string								final_quest_name;
		QuestItem							*final_quest;
		SceneVariableManager				*variable_manager;
		QuestItemsMap						all_quests;
		QuestItemsMap						pick_inventory_items_quests;
		vector<ApplyInventoryItemQuest*>	apply_invetory_items_quests;
		vector<ConditionQuest*>				condition_quests;
		vector<DialogQuest*>				dialog_quests;
		vector<ClickQuest*>					click_quests;
		vector<CompleteMiniGameQuest*>		complete_mini_game_quests;
		QuestsByLocationsMap				quests_by_location;
		vector<QuestItem *>					demo_final_quests;
		float								max_demo_progress;
		uint32								solved_in_demo_quest_count;
		uint32								current_game_progress;
		vector<string>						quest_name_pool;
	};

	inline const string & QuestGraph::GetFinalQuestName() const
	{
		return final_quest_name;
	}
}