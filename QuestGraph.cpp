//////////////////////////////////////////////////////////////////////////
// Copyright (C) 2002 - 2015 EleFun Games								//
//////////////////////////////////////////////////////////////////////////
// File name: QuestGraph.cpp										//
//////////////////////////////////////////////////////////////////////////

#include "QuestEngine/StableHeaders.hpp"

#include "QuestEngine/HintSystem/QuestGraph.hpp"

#include "QuestEngine/HintSystem/QuestItem.hpp"
#include "QuestEngine/HintSystem/CollectInventoryItemQuest.hpp"
#include "QuestEngine/HintSystem/ApplyInventoryItemQuest.hpp"
#include "QuestEngine/HintSystem/ConditionQuest.hpp"
#include "QuestEngine/HintSystem/DialogQuest.hpp"
#include "QuestEngine/HintSystem/ClickQuest.hpp"
#include "QuestEngine/HintSystem/CompleteMiniGameQuest.hpp"

#include "GameTools/XMLParser.hpp"

#include "System/MessageManager.hpp"
#include "System/FileBinary.hpp"


namespace QuestEngine
{
	QuestGraph::QuestGraph(SceneVariableManager *_variable_manager, const char * _quests_file_name, const char * _alternative_quests_file_name) :
		xml_node							(NULL),
		variable_manager					(_variable_manager),
		final_quest							(NULL),
		max_demo_progress					(0.0f),
		solved_in_demo_quest_count			(0),
		current_game_progress				(0)
	{
		Load(_quests_file_name, _alternative_quests_file_name);
	}

	QuestGraph::~QuestGraph()
	{
		erase_map(all_quests);

		delete xml_node;
	}

	void QuestGraph::Load( const char * _file_name, const char * _alternative_quests_file_name)
	{
		try
		{
			XMLParser parser;
			parser.Load(_file_name, &xml_node);
		}
		catch (const FileError &_error)
		{
			message_manager->WriteFatalError(_error.GetText());
		}

		if(!xml_node->IsAttribute("final_quest"))
		{
			message_manager->WriteFatalError(to_wstr("final_quest attribute not found in " + string(_file_name)));
		}

		final_quest_name = xml_node->GetAttributeValue("final_quest");

        // Load all quests for all locations.
        
		for (uint32 location_index = 0; location_index < xml_node->GetChildCount(); ++location_index)
		{
			XMLNodeA *location_node	= xml_node->GetChildByIndex(location_index);

			for (uint32 i = 0; i < location_node->GetChildCount(); ++i)
			{
				XMLNodeA *item_node	= location_node->GetChildByIndex(i);

				if(!item_node->IsAttribute("type"))
				{
					message_manager->WriteFatalError(to_wstr("type attribute not found in " + string(_file_name)));
				}

				AddQuestItem(item_node);
			}
		}

		for(uint32 i = 0; i < quest_name_pool.size(); i++)
		{
			QuestItem* item = GetQuestItem(quest_name_pool[i]);
			item->SetDependecies(all_quests);
		}

		quest_name_pool.clear();
		final_quest	= GetQuestItem(final_quest_name);

        // Load special quests for demo part.
        
		if(xml_node->IsAttribute("demo_final_quests"))
		{
			string data = xml_node->GetAttributeValue("demo_final_quests");
			vector<string> quest_names;
			split_string(data, '|', quest_names);
			demo_final_quests.reserve(quest_names.size());
			for (vector<string>::const_iterator it = quest_names.begin(); it != quest_names.end(); ++it)
			{
				QuestItem *quest = GetQuestItem(*it);
				if (quest)
				{
					demo_final_quests.push_back(quest);
				}
			}

			xml_node->GetAttributeValue("max_demo_progress", &max_demo_progress);
		}
	}

	void QuestGraph::UpdateConditionQuests()
	{
		for (uint32 i = 0; i < condition_quests.size(); ++i)
		{
			condition_quests[i]->ResolveConditions();
		}
	}

	QuestItem * QuestGraph::GetLastRequiredQuest( const string &_dst_task, QuestChain &_chain )
	{
		_chain.clear();
		ResetCheckedState();

		return SearchQuest(GetQuestItem(_dst_task), _chain);
	}

	QuestItem * QuestGraph::SearchQuest( QuestItem *_current_task, QuestChain &_chain )
	{
		_current_task->Check(true);

		if (!_current_task->IsSolved())
		{
			// If quest is not solved, check if all it's parents are completed. It means that last unsolved quest found.

			uint32 parent_count = _current_task->GetParentQuestsCount();

			if (parent_count > 0)
			{
				QuestItem *first_unsloved_parent = NULL;

				for (uint32 i = 0; i < parent_count  && !first_unsloved_parent; ++i)
				{
					QuestItem *parent_quest	= _current_task->GetParentQuest(i);

					if (!parent_quest->IsChecked())
					{
						if (!parent_quest->IsSolved())
						{
							// Unsolved parent found.
							first_unsloved_parent = parent_quest;
							break;
						}
					}
				}

				if (first_unsloved_parent)
				{
                    // Add parent to unsolved quests chain and continue to search.
                    
					_chain.push_back(first_unsloved_parent);
					QuestItem *last_unsolved_quest = SearchQuest(first_unsloved_parent, _chain);
						
					return last_unsolved_quest;
				}
				else
				{
					return _current_task;
				}
			}
			else
			{
				return _current_task;
			}
		}
		else
		{
			return NULL;
		}
	}

    // Qest is to pick up some item.
	CollectInventoryItemQuest * QuestGraph::GetCollectInventoryItemQuest( const string &_inventory_item )
	{
		QuestItemsMap::iterator it	= pick_inventory_items_quests.find(_inventory_item);
		CollectInventoryItemQuest *quest = NULL;

		if (it != pick_inventory_items_quests.end())
		{
			quest = dynamic_cast<CollectInventoryItemQuest*>(it->second);
		}

		return quest;
	}

    // Quest is to apply item to some area.
	ApplyInventoryItemQuest* QuestGraph::GetApplyInventoryItemQuest( const string &_inventory_item_name, const string &_drop_zone_name )
	{
		ApplyInventoryItemQuest *quest	= NULL;

		for (uint32 i = 0; i < apply_invetory_items_quests.size() && !quest; ++i)
		{
			if (apply_invetory_items_quests[i]->GetApplyDropZoneName() == _drop_zone_name &&
				apply_invetory_items_quests[i]->GetInventoryItemName() == _inventory_item_name)
			{
				quest = apply_invetory_items_quests[i];
			}
		}

		return quest;
	}

    // Quest is to click some area.
	ClickQuest * QuestGraph::GetClickQuest( const string &_layer_name, const string &_scene_name )
	{
		for (uint32 i = 0; i < click_quests.size(); ++i)
		{
			if (click_quests[i]->GetClickLayerName() == _layer_name && click_quests[i]->GetLocationName() == _scene_name)
			{
				return click_quests[i];
			}
		}

		return NULL;
	}
	
	QuestItem * QuestGraph::AddQuestItem( XMLNodeA *_xml_node)
	{
		//Add quest name to quest pool.
        
		quest_name_pool.push_back(_xml_node->GetName());
		QuestItem *item	= GetQuestItem(_xml_node->GetName());

		if (item)
		{
			message_manager->WriteFatalError(L"Hint system: Quest '" + to_wstr(_xml_node->GetName()) + L"' already exist!");
		}
			
		const string &type	= _xml_node->GetAttributeValue("type"); 
		const string &name	= _xml_node->GetName();

		if (type == "inventory_item")
		{
			item	= new CollectInventoryItemQuest(_xml_node);
			string inventory_item_name	= (static_cast<CollectInventoryItemQuest*>(item))->GetInventoryItemName();
			QuestItemsMap::iterator it	 = pick_inventory_items_quests.find(inventory_item_name);

			if (it == pick_inventory_items_quests.end())
			{
				pick_inventory_items_quests.insert(QuestItemsPair(inventory_item_name, item));
			}
			else
			{
				delete item;
				item	= NULL;

				message_manager->WriteFatalError(L"Hint system: Get inventory item quest with item '" + to_wstr(inventory_item_name) + L"' is already exist!");
			}

		}
		else if (type == "apply_item")
		{
			item = new ApplyInventoryItemQuest(_xml_node);
			apply_invetory_items_quests.push_back(static_cast<ApplyInventoryItemQuest*>(item));
		}
		else if (type == "condition")
		{
			item = new ConditionQuest(_xml_node, variable_manager);
			condition_quests.push_back(static_cast<ConditionQuest*>(item));
		}
		else if (type == "click")
		{
			item = new ClickQuest(_xml_node);
			click_quests.push_back(static_cast<ClickQuest*>(item));
		}
		else
		{
			message_manager->WriteError(L"Hints: Unknown quest type '" + to_wstr(name) + L"'!");
		}

		all_quests.insert(QuestItemsPair(item->GetName(), item));

        // Additionally fill quests_by_location section for quick access.
		const string &location_name	= _xml_node->GetParent()->GetName();
		QuestsByLocationsMap::iterator it	= quests_by_location.find(location_name);

		if (it != quests_by_location.end())
		{
			it->second.push_back(item);
		}
		else
		{
			QuestList new_list;
			new_list.push_back(item);
			quests_by_location.insert(pair<string, QuestList>(location_name, new_list));
		}

		return item;
	}

	QuestItem * QuestGraph::GetQuestItem( const string &_name )
	{
		QuestItem *item	 = NULL;
		QuestItemsMap::iterator it = all_quests.find(_name);

		if (it != all_quests.end()	)
		{
			item = it->second;
		}
			
		return item;
	}

	void QuestGraph::ResetCheckedState()
	{
		for (QuestItemsMap::iterator it = all_quests.begin(); it != all_quests.end(); ++it)
		{
			it->second->Check(false);
		}
	}

	bool QuestGraph::IsAnyAvailableQuestOnLocation( const string &_location_name )
	{
		const QuestList *quests_on_location	= GetAllQuestsOnLocation(_location_name);

		if (quests_on_location)
		{
			for (uint32 i = 0; i < quests_on_location->size(); ++i)
			{
				if (!(*quests_on_location)[i]->IsSolved() && (*quests_on_location)[i]->IsAllParentsSolved())
				{
					return true;
				}
			}
		}

		return false;
	}

	const QuestList * QuestGraph::GetAllQuestsOnLocation( const string &_location_name )
	{
		QuestsByLocationsMap::iterator it = quests_by_location.find(_location_name);

		if (it != quests_by_location.end())
		{
			return &(it->second);
		}
		
		return NULL;	
	}

	void QuestGraph::UpdateQuestLineProgress()
	{
		uint32 total_game_progress_quests_count = 0;
		uint32 solved_game_progress_quests_count = 0;

        // Count all qests and solved ones.
		for (QuestItemsMap::const_iterator it = all_quests.begin(); it != all_quests.end(); ++it)
		{
			if (!it->second->IsInactiveForGameProgress())
			{
				total_game_progress_quests_count++;

				if (it->second->IsSolved())
				{
					solved_game_progress_quests_count++;
				}	
			}
		}

        bool is_demo_over = true;
		for (vector<QuestItem *>::const_iterator it = demo_final_quests.begin(); it != demo_final_quests.end(); ++it)
		{
			is_demo_over = is_demo_over && (*it)->IsSolved();
		}
        
        // Progress for full version game and for demo cut.
		if (is_demo_over)
		{
			current_game_progress = static_cast<uint32>(max_demo_progress + (100 - max_demo_progress) *
				(solved_game_progress_quests_count - solved_in_demo_quest_count) / 
				(total_game_progress_quests_count - solved_in_demo_quest_count));
		}
		else
		{
            // 100% of solved quests in demo means progress through demo part ONLY.
			uint32 total = 0;
			uint32 solved = 0;

			deque<QuestItem *> deq;
			deq.assign(demo_final_quests.begin(), demo_final_quests.end());
			set<QuestItem *> qset;
			qset.insert(demo_final_quests.begin(), demo_final_quests.end());

            // Count demo part and cut off full game quests.
			while (deq.size() > 0)
			{
				QuestItem *current = deq.front();
				deq.pop_front();

				if (current->IsInactiveForGameProgress())
				{
					continue;
				}

				if (current->HasParents())
				{
					for (uint32 i = 0, s = current->GetParentQuestsCount(); i < s; ++i)
					{
						QuestItem *parent = current->GetParentQuest(i);
						if (qset.count(parent) == 0)
						{
							deq.push_back(parent);
							qset.insert(parent);
						}
					}
				}

				if (current->IsSolved())
				{
					++solved;
				}
				++total;
			}

			float quest_price = max_demo_progress / (total + (solved_game_progress_quests_count - solved));
			current_game_progress = static_cast<uint32>(quest_price * solved_game_progress_quests_count);

			solved_in_demo_quest_count = solved_game_progress_quests_count;
		}
	}

	uint32 QuestGraph::GetQuestLineProgress() const
	{
        uint32 progress;
        final_quest && final_quest->IsSolved() ? progress = 100 : progress = current_game_progress;
        return progress;
	}
}