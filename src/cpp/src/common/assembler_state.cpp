// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024, Advanced Micro Devices, Inc. All rights reserved.

#include "assembler_state.h"
#include "aiebu_error.h"

namespace aiebu {

assembler_state::
assembler_state(std::shared_ptr<std::map<std::string,
                std::shared_ptr<isa_op>>> isa,
                std::vector<std::shared_ptr<asm_data>>& data)
                : m_isa(isa), m_data(data)
{
  process();
  //printstate();
}

void
assembler_state::
process()
{
  code_section csection = code_section::text;
  uint32_t index = 0;
  uint32_t eopnum = 0;
  std::string clabelname;
  jobid_type cjob_id = -1;
  for (auto data : m_data)
  {

    if (data->isLabel())
    {
      csection = code_section::data;
      clabelname = data->get_operation()->get_name();
      // TODO: assert( data.get_operation()->get_name() in labelmap
      data->set_size(0);
      m_labelmap[clabelname] = std::make_shared<label>(clabelname, m_pos, index);
    } else if (data->isOpcode()){
      std::string name = data->get_operation()->get_name();
      if (!name.compare("start_job") || !name.compare("start_job_deferred"))
      {
        clabelname = "";
        cjob_id = parse_num_arg(data->get_operation()->get_args()[0]);
        //TODO asset is job already thr
        m_jobmap[cjob_id] = std::make_shared<job>(cjob_id, m_pos, index, eopnum, !name.compare("start_job_deferred"));
        m_jobids.push_back(cjob_id);
      }

      if (!name.compare("eof"))
      {
        m_jobmap[EOF_ID] = std::make_shared<job>(EOF_ID, m_pos, index, eopnum, false);
        m_jobids.push_back(EOF_ID);
      }

      if ((*m_isa).count(name) > 0)
      {
        offset_type size = (*m_isa)[name]->serializer(data->get_operation()->get_args())->size(*this);
        m_pos += size;
        data->set_size(size);
        if (!name.compare("eof"))
        {
          m_jobmap[EOF_ID]->set_end(m_pos);
          m_jobmap[EOF_ID]->set_end_index(index);
          cjob_id = -1;
        }
      } else if (!name.compare(".eop")) {
        m_jobmap[EOP_ID - eopnum] = std::make_shared<job>(EOP_ID, m_pos, index, eopnum, false);
        m_jobids.push_back(EOP_ID - eopnum);
        ++eopnum;
      } else
        throw error(error::error_code::internal_error, "Invalid operation:" + name);

      if (!name.compare("local_barrier"))
      {
        barrierid_type lbid = parse_barrier(data->get_operation()->get_args()[0]);
        auto it = m_localbarriermap.find(lbid);
        if (it == m_localbarriermap.end())
          m_localbarriermap[lbid] = std::vector<jobid_type>();
        m_jobmap[cjob_id].get()->m_barrierids.push_back(lbid);
        m_localbarriermap[lbid].push_back(cjob_id);
      }

      if (!name.compare("launch_job"))
      {
        jobid_type launchjobid = parse_num_arg(data->get_operation()->get_args()[0]);
        m_jobmap[cjob_id]->m_dependentjobs.push_back(launchjobid);
        auto it = m_joblaunchmap.find(launchjobid);
        if (it == m_joblaunchmap.end())
          m_joblaunchmap[launchjobid] = std::vector<jobid_type>();
        m_joblaunchmap[launchjobid].push_back(cjob_id);
      }

      if (!name.compare("end_job"))
      {
        //TODO : assert if cjob_id not in m_jobmap
        m_jobmap[cjob_id]->set_end(m_pos);
        m_jobmap[cjob_id]->set_end_index(index);
        cjob_id = -1;
      }

    } else {
      throw error(error::error_code::internal_error, "Unknown type found!!!");
    }

    if (!clabelname.empty() && data->get_operation()->get_name().compare(".align") && data->get_operation()->get_name().compare(".eop"))
    {
      m_labelmap[clabelname]->increment_count(1);
      m_labelmap[clabelname]->increment_size(data->get_size());
    }
    ++index;
    data->set_section(csection);
  }

  //TODO launch job id sanity check
}

uint32_t
assembler_state::
parse_num_arg(const std::string& str)
{
  // parse num
  if (str.rfind("@") == 0)
  {
    return m_labelmap[str.substr(1)]->get_pos();
  } else if (str.rfind("tile_") == 0)
  {
    uint32_t col = std::stoi(str.substr(5));
    uint32_t row = std::stoi(str.substr(6 + str.substr(5).rfind("_")));
    return ((col & 0x7F) << 5 | (row & 0x1F));
  } else if (str.rfind("s2mm_") == 0)
  {
    uint32_t index = std::stoi(str.substr(5));
    // TODO assert
    return index;
  } else if (str.rfind("mm2s_") == 0)
  {
    uint32_t index = std::stoi(str.substr(5));
    // TODO assert
    return 6 + index;
  } else if (str.rfind("0x") == 0)
    return std::stol(str.substr(2), 0 , 16);
  else if (is_number(str))
    return std::stoi(str);
  else
    throw symbol_exception();
}

void
assembler_state::
printstate() const
{
  //print state object
  //JOBS
  for (auto it=m_jobmap.begin(); it!=m_jobmap.end(); ++it)
    std::cout << "JOB[" << it->first << "] =>\tm_jobid:" << it->second->get_jobid()
              << "  m_start:" << it->second->get_start() << "  m_end:"
              << it->second->get_end() << "  m_start_index:" << it->second->get_start_index()
              << "  m_end_index:" << it->second->get_end_index() << "  m_eopnum:"
              << it->second->get_eopnum() << '\n';
  std::cout<<"\n";

  //LOCAL BARRIERS
  for (auto it=m_localbarriermap.begin(); it!=m_localbarriermap.end(); ++it) {
    std::cout << "LBMAP[" << it->first << "] =>\t";
    for( auto v : it->second)
      std::cout << v << ", ";
    std::cout<<"\n";
  }
  std::cout<<"\n";

  //LABELS
  for (auto it=m_labelmap.begin(); it!=m_labelmap.end(); ++it)
    std::cout << "LABELS[" << it->first << "] =>\tm_name:" << it->second->get_name()
              << "  m_pos:" << it->second->get_pos() << "  m_index:"
              << it->second->get_index() << "  m_count:" << it->second->get_count()
              << "  m_size:" << it->second->get_size() << '\n';
  std::cout<<"\n";
}

}
