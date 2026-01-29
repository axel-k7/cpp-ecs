#pragma once

#include <bitset>
#include <vector>
#include <unordered_map>
#include <cassert>
#include <memory>
#include <atomic>
#include <array>
#include <algorithm>

#include "Entity.h"

/////////////////////////////////////////////////////////////////////////

//ComponentInfo is for knowing the size of a certain component
//so size can be reserved for data-chunking in archetypes

struct ComponentInfo {
    uint32_t id;
    size_t element_size; //byte size of the component, sizeof(T)
    size_t element_alignment; //alignment reqs (4-byte or 16-byte), alignof(T)

    //function pointers to real objects destructor and move operators
    void (*destructor)(void* _object_ptr);
    void (*move)(void* _source, void* _destination);

    template<typename T>
    void setInfo(uint32_t _id);
};

//The raw component array, this is used for storage of
//multiple types which may not be known at compile time
//an array of iComponentArrays for example, can hold
//both a ComponentArray<Transform> and ComponentArray<Model>

//raw memory management! SCARY
//"new T()" allocates memory, then it calls the constructor
//for this type erased array, those two steps need to be separated
//due to the fact that we don't necessarily know what the constructor
//is at compile time

//have changed this now, the chunk itself handles the actual memory
//ComponentArray is now more like a view for it

struct ComponentArray {
    ComponentArray(void* _data_buffer, const ComponentInfo* _info);

    void* data_buffer;  //actual data location
    const ComponentInfo* info;

    //could just use an operator[], but [i].get(i2) looks better than [i][i2]
    void* get(size_t _index);
};

//component view is what the actual end systems want
//easier to read ComponentView<Position>
//basically just a wrapper that's not entirely necessary

template<typename T>
struct ComponentView {
    ComponentView(ComponentArray& _component_array);

    ComponentArray& component_array;

    auto operator[](size_t _index) -> T&;
    auto operator[](size_t _index) const -> const T&;
};


/////////////////////////////////////////////////////////////////////////

//chunk size is currently capacity * total size of an entity + components
//should probably change this to a set size in memory for consistency
//or if expected chunk size > limit, use a set size
//if there's like one entity with a massive amount of components
constexpr size_t CHUNK_CAPACITY = 25;

constexpr size_t MAX_COMPONENTS = 64;
using Signature = std::bitset<MAX_COMPONENTS>;

struct SignatureHash {
    size_t operator()(const Signature& _signature) const {
        return std::hash<unsigned long long>{}(_signature.to_ullong());
    }
};

class Registry {
public:
    /////////////////////////////////////////////////////////////////////////

    struct Chunk {
        Chunk(const std::vector<const ComponentInfo*> _components, size_t _capacity);
        ~Chunk();

        //disable copying
        Chunk(const Chunk&) = delete;
        Chunk& operator=(const Chunk&) = delete;

        //only allow moving, chunk controls memory on its own
        Chunk(Chunk&& _source);
        auto operator=(Chunk&& _source) -> Chunk&;

        //chunk handles memory
        void* chunk_buffer;
        void* entity_buffer; //need to keep track of where the entities are, component arrays know where they are after construction
        std::vector<ComponentArray> component_arrays;

        size_t count = 0;
        size_t capacity;

        size_t highest_alignment;

        //pure memory relocation
        void moveComponent( size_t _index, size_t _array_index,
            Chunk* _source, size_t _source_index, size_t _source_array_index,
            const ComponentInfo* _info
        );
        
        template<typename T>
        void createAt(size_t _index, size_t _array_index, T&& _data);
        void destroyAt(size_t _index, const std::vector<const ComponentInfo*>& _components);

        //swap and pop returns swapped entity
        auto swapPop(size_t _index) -> Entity;

        auto getRaw(size_t _index, size_t _array_index) -> void*;
        auto getEntities() -> Entity*; //loop over this like a C array using count

        void pushEntity(Entity _entity);
    };

    /////////////////////////////////////////////////////////////////////////

    struct Archetype {
        const Signature signature;
        std::vector<Chunk> chunks;

        std::vector<const ComponentInfo*> active_components;
        std::unordered_map<uint32_t, size_t> id_to_index;

        Archetype(const Signature& _signature, const Registry* _registry);

        auto ensureChunk() -> Chunk&;

        template<typename T>
        auto getLocalIndex() const -> size_t;
        auto getLocalIndex(uint32_t _id) const -> size_t;
    };

    /////////////////////////////////////////////////////////////////////////

    //Systems do a "query subscription" for certain components
    //this adds a query to the query list. 
    //Whenever a new archetype is added, every query checks too see 
    //if the new archetype matches the queried components
    
    //alternative to a "query cache" present in my previous ecs
    //point of this is to not have systems have to do lookups each frame
    
    //should maybe start using smart pointers
    struct Query {

        struct iView {
            //type erased view so query can communicate with all types of views
            virtual ~iView() = default;
            virtual void pushArchetype(Archetype* _archetype) = 0;

            iView(Query* _owner) 
                : query_owner(_owner)
            { }

            Query* query_owner;
            size_t view_index;
        };

        template<typename... Components>
        struct QueryView : iView {
            struct Match {
                Archetype* archetype;
                size_t component_indices[sizeof...(Components)];
            };

            struct Iterator {
                Iterator(const std::vector<Match>* _matches, size_t _match_index)
                    : matches(_matches)
                    , match_index(_match_index)
                {
                    //increment here to get to the first valid entity
                    increment();
                }

                const std::vector<Match>* matches;
                size_t match_index;
                size_t chunk_index = 0;
                size_t entity_index = 0;

                //helper function which uses std::index_sequence to
                //get components at current index in the correct order
                //ex. querying for [pos, vel] and [vel, pos] both work
                template<size_t... Indices>
                auto getTuple(std::index_sequence<Indices...>) {
                    const auto& match = (*matches)[match_index];
                    Chunk& chunk = match.archetype->chunks[chunk_index];

                    return std::forward_as_tuple(
                        chunk.getEntities()[entity_index],
                        (*static_cast<Components*>(
                            chunk.component_arrays[match.component_indices[Indices]].get(entity_index))
                        )...
                    );
                }

                void increment() {
                    //complexity here is to check for empty chunks and such

                    while (match_index < matches->size()) {
                        const auto& match = (*matches)[match_index];
                        const auto& chunks = match.archetype->chunks;


                        if (chunk_index < chunks.size() && chunks[chunk_index].count != 0) {
                            //chunk is within bounds

                            if (entity_index < chunks[chunk_index].count)
                                return; //entity is within bounds

                            //end of chunk, go to next
                            entity_index = 0;
                            chunk_index++;
                        }
                        else {
                            //end of archetype, go to next
                            chunk_index = 0;
                            match_index++;
                        }
                    }
                }


                //need to define these operators so "for ( auto a : b )" works 
                //(tuple iteration)

                auto operator*() {
                    return getTuple(std::index_sequence_for<Components...>{});
                }

                auto operator++() -> Iterator& {
                    entity_index++;
                    increment();

                    return *this;
                }

                auto operator!=(const Iterator& _other) const -> bool {
                    if (match_index >= matches->size() && _other.match_index >= _other.matches->size())
                        return false;

                    return  match_index != _other.match_index ||
                        chunk_index != _other.chunk_index ||
                        entity_index != _other.entity_index;
                }
            };

            QueryView(Query* _owner) 
                : iView(_owner)
            {
                matches.reserve(query_owner->matching_archetypes.size());

                for (auto* archetype : query_owner->matching_archetypes) {
                    pushArchetype(archetype);
                }

                view_index = query_owner->registerView(this);
            }

            ~QueryView() {
                query_owner->unregisterView(this);
            }

            void pushArchetype(Archetype* _archetype) {
                Match match{ _archetype };
                size_t i = 0;
                ((match.component_indices[i++] = _archetype->getLocalIndex<Components>()), ...);

                matches.push_back(std::move(match));
            }

            auto begin() -> Iterator { return { &matches, 0 }; }
            auto end() -> Iterator { return { &matches, matches.size()}; }

            std::vector<Match> matches;
        };



        Query(Signature _signature);

        const Signature signature;
        std::vector<Archetype*> matching_archetypes;
        std::vector<iView*> views;

        void tryMatch(Archetype* _archetype);

        auto registerView(iView* _view) -> size_t{
            views.push_back(_view);
            return views.size()-1;
        }

        //swap and pop
        void unregisterView(iView* _view) {
            size_t index = _view->view_index;

            views[index] = views.back();
            views[index]->view_index = index;
            
            views.pop_back();
        }

        template<typename... Components>
        auto view() -> QueryView<Components...> {
             return QueryView<Components...>(matching_archetypes);
        }

        
        template<typename... Components>
        auto view() const -> const QueryView<const Components...> {
            return const QueryView<const Components...>(matching_archetypes);
        }

    };

    /////////////////////////////////////////////////////////////////////////

    struct EntityRecord {
        Archetype* archetype;
        Chunk* chunk;
        size_t index;
    };

    /////////////////////////////////////////////////////////////////////////

    std::unordered_map<Signature, Archetype*, SignatureHash> signature_map;

    std::vector<std::unique_ptr<Archetype>> archetypes;
    std::vector<std::unique_ptr<Query>> query_subscriptions;

    //atomic = multiple threads can use it at the same time
    static inline std::atomic<uint32_t> type_id;

    static ComponentInfo* info_list[MAX_COMPONENTS];

    uint32_t next_id = 0;
    std::vector<uint32_t> free_ids;
    std::vector<uint32_t> versions;
    std::vector<EntityRecord> records;

    template<typename T>
    static inline auto getComponentTypeID() -> uint32_t;
    auto getComponentInfo(uint32_t _component_id) const -> ComponentInfo*;
    auto ensureArchetype(const Signature _signature) -> Archetype*;

    auto entityExists(const Entity& _entity) -> const bool;
    auto allocateEntity() -> Entity;
    void destroyEntity(Entity _entity);

    void updateEntityRecord(Entity _entity, Archetype* _archetype, Chunk* _chunk, size_t _chunk_index);
    void invalidateEntity(const Entity& _entity);

    void eraseChunkEntry(Chunk* _chunk, size_t _index);

    template<typename... Components> void addComponents(Entity _entity, Components&&... _data);
    template<typename... Components> void removeComponents(Entity _entity);
    void moveEntity(Entity _entity, const Signature& _target_signature);

    template<typename Component> auto tryGetComponent(Entity _entity) -> Component*;

    template<typename Excluded>
    struct Exclude{};

    template<typename... Components>
    auto query() -> Query::QueryView<Components...>;

    template<typename... Included, typename... Excluded>
    auto query(Exclude<Excluded...>) -> Query::QueryView<Included...>;

    auto query(const Signature _signature) -> Query*;

    template<typename... T>
    using QueryResult = Query::QueryView<T...>;
};

//TODO
//General
//  *   look into component inheritance some more, idk if worth cause of cache innefficiency
//      shared components maybe?
//  
//  *   multithreading, need to know which systems "set" vs "get" components (wait for lesson)
// 
//  *   always look for api improvements
// 
//  *   handling different queries of different types, event + component query = inefficient in current sys
// 
//  *   handling state, active & inactive registry? (expensive)
// 
//  *   update command buffer, maybe wait for multithreading implementation first
// 
//Registry 
//  *   singleton components, like delta time
// 
//  *   check how tag components / empty structs work with chunk memory management
// 
//  *   helper function to move from one archetype to another with a single call
//      needed for when calculating final signature in the buffer by adding 
//      all the "add" and "remove" component calls together
// 
//  *   override/ensureComponents helper, would guarrantee that an entity has a component with input values in 1 call
//      entity has a PositionComponent with x = 10 
//      user calls ensureComponent<Position>(50,0,0) 
//      entity WILL have a PositionComponent with x = 50
// 
// 
//System
//  *   can currently cache archetypes via subscription
//      maybe add some way to cache component indexes for each archetype?
//      would skip getLocalIndex calls every frame
//



#include "RegistryNEW.inl"