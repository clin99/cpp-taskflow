#pragma once

#include "flow_builder.hpp"

namespace tf {

class WorkGroup;

/**
@class Framework 

@brief A reusable task dependency graph.

A framework is a task dependency graph that is independent
of a taskflow object. You can run a framework multiple times 
from a taskflow object to enable a reusable control flow.

*/
class Framework : public FlowBuilder {

  template <template<typename...> typename E> 
  friend class BasicTaskflow;
  
  friend class Topology;
  friend class WorkGroup;

  public:

    /**
    @brief constructs the framework with an empty task dependency graph
    */
    Framework();

    /**
    @brief destroy the framework (virtual call)
    */
    virtual ~Framework();
    
    /**
    @brief dumps the framework to a std::ostream in DOT format

    @param ostream a std::ostream target
    */
    void dump(std::ostream& ostream) const;
    
    /**
    @brief dumps the framework in DOT format to a std::string
    */
    std::string dump() const;
    
    /**
    @brief queries the number of nodes in the framework
    */
    size_t num_nodes() const;

    auto& name(const std::string&) ;

    const std::string& name() const ;

  private:

    std::string _name;
    
    Graph _graph;

    std::mutex _mtx;
    std::list<Topology*> _topologies;
};

// Constructor
inline Framework::Framework() : FlowBuilder{_graph} {
}

// Destructor
inline Framework::~Framework() {
  assert(_topologies.empty());
}

// Function: name
inline auto& Framework::name(const std::string &name) {
  _name = name;
  return *this;
}

// Function: name
inline const std::string& Framework::name() const {
  return _name;
}



// Function: num_noces
inline size_t Framework::num_nodes() const {
  return _graph.size();
}

// Procedure: dump
inline void Framework::dump(std::ostream& os) const {
  os << "digraph " << _name << " Framework {\n";
  for(const auto& n: _graph) {
    n.dump(os);
  }
  os << "}\n";
}

// Function: dump
inline std::string Framework::dump() const { 
  std::ostringstream os;
  dump(os);
  return os.str();
}



class WorkGroup : public FlowBuilder {

  friend class Topology;
  
  template <template<typename...> typename E>  
  friend class BasicTaskflow;

  public:

    WorkGroup();

    virtual ~WorkGroup();

    using FlowBuilder::silent_emplace;
    using FlowBuilder::emplace;
    tf::Task emplace(Framework& framework);

    void dump(std::ostream& ostream) const;
    
    std::string dump() const;


  protected:

    Graph _graph;

  private:

    std::vector<std::pair<Node*, Framework*>> _pairs;

    std::mutex _mtx;
    std::list<Topology*> _topologies;
    Node* _last_target {nullptr};   

    size_t _now_iteration;
};

// Constructor
inline WorkGroup::WorkGroup() : FlowBuilder{_graph} {
}

// Destructor
inline WorkGroup::~WorkGroup() {
  assert(_topologies.empty());
}


inline tf::Task WorkGroup::emplace(Framework& framework) {
  auto task = placeholder();
  _graph.back().set_workgroup();
  _pairs.emplace_back(&(_graph.back()), &framework);
  if(!framework.name().empty()) {
    task.name(framework.name());
  }
  return task;
}

// Procedure: dump
inline void WorkGroup::dump(std::ostream& os) const {
  os << "digraph WorkGroup {\n";
  for(const auto& n: _graph) {
    n.dump(os);
  }
  os << "}\n";
}

// Function: dump
inline std::string WorkGroup::dump() const { 
  std::ostringstream os;
  dump(os);
  return os.str();
}


}  // end of namespace tf. ---------------------------------------------------

