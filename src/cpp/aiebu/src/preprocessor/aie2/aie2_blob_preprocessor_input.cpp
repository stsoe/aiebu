// SPDX-License-Identifier: MIT
// Copyright (C) 2024, Advanced Micro Devices, Inc. All rights reserved.

#include "aie2_blob_preprocessor_input.h"
#include "xaiengine.h"

namespace aiebu {


  /*
  sample json
  {
      "external_buffers": {
          "buffer0": {
              "xrt_id": 1,
              "size_in_bytes": 345088,
              "name": "coalesed_weights",
              "coalesed_buffers": [
                  {
                      "logical_id": 0,
                      "offset_in_bytes": 0,
                      "name": "compute_graph.resnet_layers[0].wts_ddr",
                      "control_packet_patch_locations": [
                          {
                              "offset": 17420,
                              "size": 6,
                              "operation": "read_add_write"
                          },
                          {
                              "offset": 17484,
                              "size": 6,
                              "operation": "read_add_write"
                          },
                          {
                              "offset": 17548,
                              "size": 6,
                              "operation": "read_add_write"
                          },
                          {
                              "offset": 17612,
                              "size": 6,
                              "operation": "read_add_write"
                          }
                      ]
                  },
                  {
                      "logical_id": 1,
                      "offset_in_bytes": 37888,
                      "name": "compute_graph.resnet_layers[1].wts_ddr",
                      "control_packet_patch_locations": [
                          {
                              "offset": 19404,
                              "size": 6,
                              "operation": "read_add_write"
                          },
                          {
                              "offset": 19468,
                              "size": 6,
                              "operation": "read_add_write"
                          },
                          {
                              "offset": 19532,
                              "size": 6,
                              "operation": "read_add_write"
                          },
                          {
                              "offset": 19596,
                              "size": 6,
                              "operation": "read_add_write"
                          }
                      ]
                  },
                  {
                      "logical_id": 2,
                      "offset_in_bytes": 195584,
                      "name": "compute_graph.resnet_layers[2].wts_ddr",
                      "control_packet_patch_locations": [
                          {
                              "offset": 40012,
                              "size": 6,
                              "operation": "read_add_write"
                          },
                          {
                              "offset": 40076,
                              "size": 6,
                              "operation": "read_add_write"
                          },
                          {
                              "offset": 40140,
                              "size": 6,
                              "operation": "read_add_write"
                          },
                          {
                              "offset": 40204,
                              "size": 6,
                              "operation": "read_add_write"
                          }
                      ]
                  }
              ]
          },
          "buffer1": {
              "xrt_id": 2,
              "logical_id": 3,
              "size_in_bytes": 802816,
              "name": "compute_graph.ifm_ddr",
              "control_packet_patch_locations": [
                  {
                      "offset": 12,
                      "size": 6,
                      "operation": "read_add_write"
                  },
                  {
                      "offset": 76,
                      "size": 6,
                      "operation": "read_add_write"
                  }
              ]
          },
          "buffer2": {
              "xrt_id": 3,
              "logical_id": 4,
              "size_in_bytes": 458752,
              "name": "compute_graph.ofm_ddr",
              "control_packet_patch_locations": [
                  {
                      "offset": 60428,
                      "size": 6,
                      "operation": "read_add_write"
                  },
                  {
                      "offset": 60492,
                      "size": 6,
                      "operation": "read_add_write"
                  }
              ]
          },
          "buffer3": {
              "xrt_id": 0,
              "logical_id": -1,
              "size_in_bytes": 60736,
              "ctrl_pkt_buffer": 1,
              "name": "runtime_control_packet"
          }
      }
  }
  */

  void
  aie2_blob_preprocessor_input::
  validate_json(uint32_t offset, uint32_t size, uint32_t arg_index, offset_type type) const {
    // Return if the offset and arg_index are within their respective sizes.
    if ((offset <= size) && (arg_index <= MAX_ARG_INDEX)) {
      return;
    }
    std::string errorMessage;
    if (offset > size ) {
      errorMessage = std::string("INVALID JSON: Offset(")
      + std::to_string(offset)
      + ") is greater than size("
      + std::to_string(size)
      + ") for offset Type: "
      + (type == offset_type::CONTROL_PACKET ? "CONTROL PACKET" : "BUFFER")
      + " and arg index is "
      + (arg_index > MAX_ARG_INDEX ? "INVALID = " : "VALID = ")
      + std::to_string(arg_index) + ". ";
    }
    else {
      errorMessage = std::string("INVALID JSON: arg index (")
      + std::to_string(arg_index)
      + ") is greater than Max arg index ="
      + std::to_string(MAX_ARG_INDEX)
      + ". ";
    }
    throw error(error::error_code::invalid_asm, errorMessage);
  }

  void
  aie2_blob_preprocessor_input::
  extract_coalesed_buffers(const std::string& name,
                           const boost::property_tree::ptree& pt)
  {
    uint32_t buffer_size = pt.get<uint32_t>("size_in_bytes");
    const auto coalesed_buffers_pt = pt.get_child_optional("coalesed_buffers");
    if (!coalesed_buffers_pt)
      return;

    const auto coalesed_buffers = coalesed_buffers_pt.get();
    for (auto coalesed_buffer : coalesed_buffers) {
      uint32_t buffer_offset = coalesed_buffer.second.get<uint32_t>("offset_in_bytes");
      uint32_t arg_index = pt.get<uint32_t>("xrt_id");
      // Check if the buffer offset is within the buffer size
      validate_json(buffer_offset, buffer_size, arg_index, offset_type::COALESED_BUFFER);
      // extract control packet patch
      extract_control_packet_patch(name, arg_index, coalesed_buffer.second);
    }
  }

  void
  aie2_blob_preprocessor_input::
  extract_control_packet_patch(const std::string& name,
                               const uint32_t arg_index,
                               const boost::property_tree::ptree& pt)
  {
    const uint32_t addend = validate_and_return_addend(pt.get<uint64_t>("offset_in_bytes", 0));
    const auto control_packet_patch_pt = pt.get_child_optional("control_packet_patch_locations");
    if (!control_packet_patch_pt)
      return;
    const auto patchs = control_packet_patch_pt.get();
    for (auto pat : patchs)
    {
      auto patch = pat.second;
      uint32_t control_packet_size = m_data[".ctrldata"].size();
      uint32_t control_packet_offset = patch.get<uint32_t>("offset");
      // Check if the control packet offset is within the control packet size
      validate_json(control_packet_offset, control_packet_size, arg_index, offset_type::CONTROL_PACKET);
      // move 8 bytes(header) up for unifying the patching scheme between DPU sequence and transaction-buffer
      uint32_t offset = patch.get<uint32_t>("offset") - 8;
      add_symbol({name, offset, 0, 0, addend, 0, ctrlData, symbol::patch_schema::control_packet_48});
    }
  }

  void
  aie2_blob_preprocessor_input::
  aiecompiler_json_parser(const boost::property_tree::ptree& pt)
  {
    const auto pt_external_buffers = pt.get_child_optional("external_buffers");
    if (!pt_external_buffers)
      return;

    const auto external_buffers = pt_external_buffers.get();
    for (auto& external_buffer : external_buffers)
    {
      const auto pt_coalesed_buffers = external_buffer.second.get_child_optional("coalesed_buffers");
      // added ARG_OFFSET to argidx to match with kernel argument index in xclbin
      auto arg = external_buffer.second.get<uint32_t>("xrt_id");
      std::string name = std::to_string(arg + ARG_OFFSET);
      if (external_buffer.second.get<bool>("ctrl_pkt_buffer", false))
        xrt_id_map.insert({arg, "control-packet"});
      else
        xrt_id_map.insert({arg, name});

      if (pt_coalesed_buffers)
        extract_coalesed_buffers(name, external_buffer.second);
      else
        extract_control_packet_patch(name, arg, external_buffer.second);
    }
  }

  void
  aie2_blob_preprocessor_input::
  dmacompiler_json_parser(const boost::property_tree::ptree& pt)
  {

    // fixed in dma compiler
    xrt_id_map.insert({0, "3"});
    xrt_id_map.insert({1, "4"});
    xrt_id_map.insert({2, "5"});
    xrt_id_map.insert({3, "6"});
    xrt_id_map.insert({4, "7"});

    const auto pt_ctrl_xrt_arg_idx = pt.get_optional<uint32_t>("ctrl_pkt_xrt_arg_idx");
    if (pt_ctrl_xrt_arg_idx)
    {
      // if "ctrl_pkt_xrt_arg_idx" present make that as controlpacket index
      xrt_id_map.insert_or_assign(pt_ctrl_xrt_arg_idx.get(), "control-packet");
    } else {
      // if "ctrl_pkt_xrt_arg_idx" not present default arg4 is controlpacket
      xrt_id_map[4] = "control-packet";
    }

    const auto pt_ctrl_pkt_patch_info = pt.get_child_optional("ctrl_pkt_patch_info");
    if (!pt_ctrl_pkt_patch_info)
      return;

    const auto patchs = pt_ctrl_pkt_patch_info.get();
    for (auto pat : patchs)
    {
      auto patch = pat.second;
      uint32_t control_packet_offset = patch.get<uint32_t>("offset");
      uint32_t control_packet_size = m_data[".ctrldata"].size();
      uint32_t arg_index = patch.get<uint32_t>("xrt_arg_idx");
      // check if the offset is less than the size of the control packet
      validate_json(control_packet_offset, control_packet_size, arg_index, offset_type::CONTROL_PACKET);
      // move 8 bytes(header) up for unifying the patching scheme between DPU sequence and transaction-buffer
      uint32_t offset = patch.get<uint32_t>("offset") - 8;
      const uint32_t addend = validate_and_return_addend(patch.get<uint64_t>("bo_offset"));
      const uint32_t arg = patch.get<uint32_t>("xrt_arg_idx");
      add_symbol({std::to_string(arg + ARG_OFFSET), offset, 0, 0, addend, 0, ctrlData, symbol::patch_schema::control_packet_48});
    }

  }

  void
  aie2_blob_preprocessor_input::
  readmetajson(std::istream& patch_json)
  {
    boost::property_tree::ptree pt;
    boost::property_tree::read_json(patch_json, pt);

    const auto aiecompiler_json = pt.get_child_optional("external_buffers");
    if (aiecompiler_json)
    {
      aiecompiler_json_parser(pt);
      return;
    }

    const auto dmacompiler_json = pt.get_child_optional("ctrl_pkt_patch_info");
    if (dmacompiler_json)
    {
      dmacompiler_json_parser(pt);
      return;
    }
  }


  uint32_t
  aie2_blob_preprocessor_input::
  validate_and_return_addend(uint64_t addend64) const
  {
    // we dont support addend greater then 32 bit
    if (addend64 > MAX_ARGPLUS)
    {
      auto error_msg = boost::format("Invalid addend (0x%x) > 32bit found") % addend64;
      throw error(error::error_code::invalid_asm, error_msg.str());
    }
    return static_cast<uint32_t>(addend64);
  }

  // 20 Lower bits
  #define GET_REG(reg) (reg & 0xFFFFF)

  void
  aie2_blob_preprocessor_input::
  clear_shimBD_address_bits(std::vector<char>& mc_code, uint32_t offset) const
  {
    constexpr static uint32_t DMA_BD_1_IN_BYTES = 1 * 4;
    constexpr static uint32_t DMA_BD_2_IN_BYTES = 2 * 4;
    //Clearing address bits as they are set at runtime during patching(xrt/firmware).
    //Lower Base Address. 30 LSB of a 46-bit long 32-bit-word-address. (bits [31:2] in DMA_BD_1 of a 48-bit byte-address)
    //Upper Base Address. 16 MSB of a 46-bit long 32-bit-word-address. (bits [47:32] in DMA_BD_2 of a 48-bit byte-address)
    mc_code[offset + DMA_BD_1_IN_BYTES] = mc_code[offset + DMA_BD_1_IN_BYTES] & (0x03);
    mc_code[offset + DMA_BD_1_IN_BYTES + 1] = mc_code[offset + DMA_BD_1_IN_BYTES + 1] & (0x00);
    mc_code[offset + DMA_BD_1_IN_BYTES + 2] = mc_code[offset + DMA_BD_1_IN_BYTES + 2] & (0x00);
    mc_code[offset + DMA_BD_1_IN_BYTES + 3] = mc_code[offset + DMA_BD_1_IN_BYTES + 3] & (0x00);
    mc_code[offset + DMA_BD_2_IN_BYTES] = mc_code[offset + DMA_BD_2_IN_BYTES] & (0x00);
    mc_code[offset + DMA_BD_2_IN_BYTES + 1] = mc_code[offset + DMA_BD_2_IN_BYTES + 1] & (0x00);
  }

  #define MAJOR_VER 1
  #define MINOR_VER 0

  uint32_t aie2_blob_transaction_preprocessor_input::process_txn(const char *ptr, std::vector<char>& mc_code, const std::string& section_name, const std::string& argname)
  {
    std::map<uint64_t,std::pair<uint32_t, uint64_t>> blockWriteRegOffsetMap;
    auto txn_header = reinterpret_cast<const XAie_TxnHeader *>(ptr);
    uint32_t loadsequence = 0;
    uint8_t pm_id = 0;

    ptr += sizeof(XAie_TxnHeader);
    for(uint32_t num = 0; num < txn_header->NumOps; num++) {
      auto op_header = reinterpret_cast<const XAie_OpHdr *>(ptr);
      switch(op_header->Op) {
        case XAIE_IO_WRITE: {
          auto w_header = reinterpret_cast<const XAie_Write32Hdr *>(ptr);
          ptr += w_header->Size;
          break;
        }
        case XAIE_IO_BLOCKWRITE: {
          auto bw_header = reinterpret_cast<const XAie_BlockWrite32Hdr *>(ptr);
          auto payload = reinterpret_cast<const char*>(ptr + sizeof(XAie_BlockWrite32Hdr));
          auto offset = static_cast<uint32_t>(payload-mc_code.data());
          uint32_t size = (bw_header->Size - sizeof(*bw_header));
          if (loadsequence > 0)
          {
            uint64_t buffer_length_in_bytes = reinterpret_cast<const uint32_t*>(payload)[0] * byte_in_word;
            patch_helper_input input = {section_name, ctrlpkt_pm + std::to_string(pm_id),
                                        static_cast<uint32_t>(GET_REG(bw_header->RegOff)+ 4),
                                        0, offset, buffer_length_in_bytes, 0};
            patch_helper(mc_code, input);
          }
          else
          {
            // we can combine multiple bd writes in one blockwrite and have patch opcodes after that
            // we divide the blockwrite in bd chuncks and add in blockWriteRegOffsetMap
            for (auto bd = 0U ; bd < size; bd += SHIM_DMA_BD_SIZE) { //size and bd in bytes
              uint64_t buffer_length_in_bytes = reinterpret_cast<const uint32_t*>(payload)[bd/byte_in_word] * byte_in_word;
              blockWriteRegOffsetMap[bw_header->RegOff + bd] = std::make_pair(offset + bd, buffer_length_in_bytes);
            }
          }
          ptr += bw_header->Size;
          break;
        }
        case XAIE_IO_MASKWRITE: {
          auto mw_header = reinterpret_cast<const XAie_MaskWrite32Hdr *>(ptr);
          ptr += mw_header->Size;
          break;
        }
        case XAIE_IO_MASKPOLL:
        case XAIE_IO_MASKPOLL_BUSY: {
          auto mp_header = reinterpret_cast<const XAie_MaskPoll32Hdr *>(ptr);
          ptr += mp_header->Size;
          break;
        }
        case XAIE_IO_NOOP: {
            ptr += sizeof(XAie_NoOpHdr);
            break;
        }
        case XAIE_IO_PREEMPT: {
            ptr += sizeof(XAie_PreemptHdr);
            break;
        }
        case XAIE_IO_LOAD_PM_START: {
          auto mp_header = (const XAie_PmLoadHdr *)(ptr);
          loadsequence = mp_header->LoadSequenceCount[2] << 16 | mp_header->LoadSequenceCount[1] << 8 | mp_header->LoadSequenceCount[0];
          loadsequence = loadsequence + 1;
          pm_id = mp_header->PmLoadId;
          if (std::find(pm_id_list.begin(), pm_id_list.end(), pm_id) == pm_id_list.end())
            throw error(error::error_code::invalid_asm, "PM id:" + std::to_string(pm_id) + " has no corresponding pm control packet !!!");
          ptr += sizeof(XAie_PmLoadHdr);
          break;
        }
        case XAIE_IO_CUSTOM_OP_BEGIN: {
          auto co_header = reinterpret_cast<const XAie_CustomOpHdr *>(ptr);
          ptr += co_header->Size;
          break;
        }
        case XAIE_IO_CUSTOM_OP_BEGIN+1: {
          auto hdr = reinterpret_cast<const XAie_CustomOpHdr *>(ptr);
          if (loadsequence)
            throw error(error::error_code::invalid_asm, "Patch opcode found in PM Load Sequence!!!");
          auto op = reinterpret_cast<const patch_op_t *>(ptr + sizeof(*hdr));
          uint64_t reg = op->regaddr & 0xFFFFFFF0; // regaddr point either to 1st word or 2nd word of BD
          auto it = blockWriteRegOffsetMap.find(reg);
          if ( it == blockWriteRegOffsetMap.end()) {
            auto error_msg = boost::format("Invalid Control Code. No block-write opcode"
            " present before the patch opcode for address 0x%x") % reg;
            throw error(error::error_code::invalid_asm, error_msg.str());
          }
          uint32_t offset = blockWriteRegOffsetMap[reg].first;
          uint64_t buffer_length_in_bytes = blockWriteRegOffsetMap[reg].second;
          patch_helper_input input = {section_name, argname, static_cast<uint32_t>(GET_REG(op->regaddr)),
                                      static_cast<uint32_t>(op->argidx + ARG_OFFSET), offset,
                                      buffer_length_in_bytes, op->argplus};
          patch_helper(mc_code, input);
          ptr += hdr->Size;
          break;
        }
        case XAIE_IO_CUSTOM_OP_BEGIN+2: {
          auto hdr = reinterpret_cast<const XAie_CustomOpHdr *>(ptr);
          ptr += hdr->Size;
          break;
        }
        case XAIE_IO_CUSTOM_OP_BEGIN+3: {
          auto hdr = reinterpret_cast<const XAie_CustomOpHdr *>(ptr);
          ptr += hdr->Size;
          break;
        }
        case XAIE_IO_CUSTOM_OP_MERGE_SYNC: {
          auto hdr = reinterpret_cast<const XAie_CustomOpHdr *>(ptr);
          ptr += hdr->Size;
          break;
        }
        default:
          throw error(error::error_code::invalid_asm, "Invalid txn opcode: " + std::to_string(op_header->Op) + " !!!");
      }

      loadsequence = loadsequence > 0 ? loadsequence-1 : 0;
    }
    return txn_header->NumCols;
  }


  uint32_t aie2_blob_transaction_preprocessor_input::process_txn_opt(const char *ptr, std::vector<char>& mc_code, const std::string& section_name, const std::string& argname)
  {
    std::map<uint32_t,std::pair<uint32_t, uint64_t>> blockWriteRegOffsetMap;
    auto txn_header = reinterpret_cast<const XAie_TxnHeader *>(ptr);
    uint32_t loadsequence = 0;
    uint8_t pm_id = 0;

    ptr += sizeof(XAie_TxnHeader);
    for(uint32_t num = 0; num < txn_header->NumOps; num++) {
      auto op_header = reinterpret_cast<const XAie_OpHdr_opt *>(ptr);
      switch(op_header->Op) {
        case XAIE_IO_WRITE: {
          ptr += sizeof(XAie_Write32Hdr_opt);
          break;
        }
        case XAIE_IO_BLOCKWRITE: {
          auto bw_header = reinterpret_cast<const XAie_BlockWrite32Hdr_opt *>(ptr);
          auto payload = reinterpret_cast<const char*>(ptr + sizeof(XAie_BlockWrite32Hdr_opt));
          auto offset = static_cast<uint32_t>(payload-mc_code.data());
          uint32_t size = (bw_header->Size - sizeof(*bw_header));
          if (loadsequence > 0)
          {
            uint64_t buffer_length_in_bytes = reinterpret_cast<const uint32_t*>(payload)[0] * byte_in_word;
            patch_helper_input input = {section_name, ctrlpkt_pm + std::to_string(pm_id),
                                        static_cast<uint32_t>(GET_REG(bw_header->RegOff)+ 4),
                                        0, offset, buffer_length_in_bytes, 0};
            patch_helper(mc_code, input);
          }
          else
          {
            // we can combine multiple bd writes in one blockwrite and have patch opcodes after that
            // we divide the blockwrite in bd chuncks and add in blockWriteRegOffsetMap
            for (auto bd = 0U ; bd < size; bd+=SHIM_DMA_BD_SIZE) { //size and bd in bytes
              uint64_t buffer_length_in_bytes = reinterpret_cast<const uint32_t*>(payload)[bd/byte_in_word] * byte_in_word;
              blockWriteRegOffsetMap[bw_header->RegOff + bd] = std::make_pair(offset + bd, buffer_length_in_bytes);
            }
          }
          ptr += bw_header->Size;
          break;
        }
        case XAIE_IO_MASKWRITE: {
          ptr += sizeof(XAie_MaskWrite32Hdr_opt);
          break;
        }
        case XAIE_IO_MASKPOLL:
        case XAIE_IO_MASKPOLL_BUSY: {
          ptr += sizeof(XAie_MaskPoll32Hdr_opt);
          break;
        }
        case XAIE_IO_NOOP: {
            ptr += sizeof(XAie_NoOpHdr);
            break;
        }
        case XAIE_IO_PREEMPT: {
            ptr += sizeof(XAie_PreemptHdr);
            break;
        }
        case XAIE_IO_LOAD_PM_START: {
          auto mp_header = (const XAie_PmLoadHdr *)(ptr);
          loadsequence = mp_header->LoadSequenceCount[2] << 16 | mp_header->LoadSequenceCount[1] << 8 | mp_header->LoadSequenceCount[0];
          loadsequence = loadsequence + 1;
          pm_id = mp_header->PmLoadId;
          if (std::find(pm_id_list.begin(), pm_id_list.end(), pm_id) == pm_id_list.end())
            throw error(error::error_code::invalid_asm, "PM id:" + std::to_string(pm_id) + " has no corresponding pm control packet !!!");
          ptr += sizeof(XAie_PmLoadHdr);
          break;
        }
        case XAIE_IO_CUSTOM_OP_TCT: {
          auto co_header = reinterpret_cast<const XAie_CustomOpHdr_opt *>(ptr);
          ptr += co_header->Size;
          break;
        }
        case XAIE_IO_CUSTOM_OP_DDR_PATCH: {
          auto hdr = reinterpret_cast<const XAie_CustomOpHdr_opt *>(ptr);
          if (loadsequence)
            throw error(error::error_code::invalid_asm, "Patch opcode found in PM Load Sequence!!!");
          auto op = reinterpret_cast<const patch_op_t *>(ptr + sizeof(*hdr));
          uint64_t reg = op->regaddr & 0xFFFFFFF0; // regaddr point either to 1st word or 2nd word of BD
          auto it = blockWriteRegOffsetMap.find(reg);
          if ( it == blockWriteRegOffsetMap.end()) {
            auto error_msg = boost::format("Invalid Control Code. No block-write opcode"
            " present before the patch opcode for address 0x%x") % reg;
            throw error(error::error_code::invalid_asm, error_msg.str());
          }
          uint32_t offset = blockWriteRegOffsetMap[reg].first;
          uint64_t buffer_length_in_bytes = blockWriteRegOffsetMap[reg].second;
          patch_helper_input input = {section_name, argname, static_cast<uint32_t>(GET_REG(op->regaddr)),
                                      static_cast<uint32_t>(op->argidx + ARG_OFFSET), offset,
                                      buffer_length_in_bytes, op->argplus};
          patch_helper(mc_code, input);
          ptr += hdr->Size;
          break;
        }
        case XAIE_IO_CUSTOM_OP_READ_REGS: {
          auto hdr = reinterpret_cast<const XAie_CustomOpHdr_opt *>(ptr);
          ptr += hdr->Size;
          break;
        }
        case XAIE_IO_CUSTOM_OP_RECORD_TIMER: {
          auto hdr = reinterpret_cast<const XAie_CustomOpHdr_opt *>(ptr);
          ptr += hdr->Size;
          break;
        }
        case XAIE_IO_CUSTOM_OP_MERGE_SYNC: {
          auto hdr = reinterpret_cast<const XAie_CustomOpHdr_opt *>(ptr);
          ptr += hdr->Size;
          break;
        }
        default:
          throw error(error::error_code::invalid_asm, "Invalid txn opcode: " + std::to_string(op_header->Op) + " !!!");
      }
    }
    return txn_header->NumCols;
  }

  uint32_t
  aie2_blob_transaction_preprocessor_input::
  extractSymbolFromBuffer(std::vector<char>& mc_code,
                          const std::string& section_name,
                          const std::string& argname)
  {
    const char *ptr = (mc_code.data());
    auto txn_header = reinterpret_cast<const XAie_TxnHeader *>(ptr);

    printf("Header version %d.%d\n", txn_header->Major, txn_header->Minor);
    printf("Device Generation: %d\n", txn_header->DevGen);
    printf("Cols, Rows, NumMemRows : (%d, %d, %d)\n", txn_header->NumCols,
         txn_header->NumRows, txn_header->NumMemTileRows);
    printf("TransactionSize: %u\n", txn_header->TxnSize);
    printf("NumOps: %u\n", txn_header->NumOps);

    /**
     * Check if Header Version is 1.0 then call optimized API else continue with this
     * function to service the TXN buffer.
     */
    if ((txn_header->Major == MAJOR_VER) && (txn_header->Minor == MINOR_VER)) {
        printf("Optimized HEADER version detected \n");
        return process_txn_opt(ptr, mc_code, section_name, argname);
    }
    return process_txn(ptr, mc_code, section_name, argname);
   }

  void
  aie2_blob_transaction_preprocessor_input::
  patch_helper(std::vector<char>& mc_code,
               const patch_helper_input& input)
  {
    const std::string& section_name = input.section_name;
    const std::string& argname = input.argname;
    uint32_t reg = input.reg;
    uint32_t argidx = input.argidx;
    uint32_t offset = input.offset;
    uint64_t buffer_length_in_bytes = input.buffer_length_in_bytes;
    uint32_t addend = validate_and_return_addend(input.addend);

    std::vector<uint32_t> MEM_BD_ADDRESS;
    for (auto i=0U; i < MEM_DMA_BD_NUM; ++i)
      MEM_BD_ADDRESS.push_back(MEM_DMA_BD0_0 + i * MEM_DMA_BD_SIZE);

    std::vector<uint32_t> SHIM_BD_ADDRESS;
    for (auto i=0U; i < SHIM_DMA_BD_NUM; ++i)
      SHIM_BD_ADDRESS.push_back(SHIM_DMA_BD0_0 + i * SHIM_DMA_BD_SIZE);

    {
      //MEM bd buffer length patch: reg point to mem bd_0
      auto it = std::find(MEM_BD_ADDRESS.begin(), MEM_BD_ADDRESS.end(), reg);
      if (it != MEM_BD_ADDRESS.end())
      {
        // size is overloaded, for scaler_32 size contain mask
        add_symbol({std::to_string(argidx), offset, 0, 0, addend, register_mask[register_id::MEM_BUFFER_LENGTH], section_name, symbol::patch_schema::scaler_32});
        return;
      }
    }

    {
      //MEM bd base address patch: reg point to mem bd_1
      auto it = std::find(MEM_BD_ADDRESS.begin(), MEM_BD_ADDRESS.end(), reg-4);
      if (it != MEM_BD_ADDRESS.end())
      {
        // size is overloaded, for scaler_32 size contain mask
        add_symbol({std::to_string(argidx), offset + 4, 0, 0, addend, register_mask[register_id::MEM_BASE_ADDRESS], section_name, symbol::patch_schema::scaler_32});
        return;
      }
    }

    {
      //SHIM bd buffer length patch: reg point to shim bd_0
      auto it = std::find(SHIM_BD_ADDRESS.begin(), SHIM_BD_ADDRESS.end(), reg);
      if (it != SHIM_BD_ADDRESS.end())
      {
        // size is overloaded, for scaler_32 size contain mask
        add_symbol({std::to_string(argidx), offset, 0, 0, addend, register_mask[register_id::SHIM_BUFFER_LENGTH], section_name, symbol::patch_schema::scaler_32});
        return;
      }
    }

    {
      //SHIM bd base address patch: reg point to shim bd_1
      auto it = std::find(SHIM_BD_ADDRESS.begin(), SHIM_BD_ADDRESS.end(), reg-4);
      if (it != SHIM_BD_ADDRESS.end())
      {
        clear_shimBD_address_bits(mc_code, offset);
        if (!argname.empty())
        {
          // in case of scratchpad
          add_symbol({argname, offset, 0, 0, addend, buffer_length_in_bytes, section_name, symbol::patch_schema::shim_dma_48});
        }
        else if (xrt_id_map.find(argidx-ARG_OFFSET) != xrt_id_map.end())
        {
          // incase external buffer json is provided with xrt_id
          add_symbol({xrt_id_map[argidx-ARG_OFFSET], offset, 0, 0, addend, buffer_length_in_bytes, section_name, symbol::patch_schema::shim_dma_48});
        }
        else
        {
          // added ARG_OFFSET to argidx to match with kernel argument index in xclbin
          add_symbol({std::to_string(argidx), offset, 0, 0, addend, buffer_length_in_bytes, section_name, symbol::patch_schema::shim_dma_48});
        }
        return;
      }
    }
  }

  void
  aie2_blob_dpu_preprocessor_input::
  patch_shimbd(const uint32_t* instr_ptr, size_t pc, const std::string& section_name)
  {
    uint32_t regId = (instr_ptr[pc] & 0x000000F0) >> 4;
    static std::map<uint32_t, std::string> arg2name = {
      {0, "ifm"},
      {1, "param"},
      {2, "ofm"},
      {3, "inter"},
      {4, "out2"},
      {5, "control-packet"}
    };

    auto it = arg2name.find(regId);
    if ( it == arg2name.end() )
      throw error(error::error_code::invalid_asm, "Invalid dpu arg:" + std::to_string(regId) + " !!!");

    uint32_t offset = static_cast<uint32_t>((pc+1)*4); //point to start of BD
    add_symbol({arg2name[regId], offset, 0, 0, 0, 0, section_name, symbol::patch_schema::shim_dma_48});
  }

  uint32_t
  aie2_blob_dpu_preprocessor_input::
  extractSymbolFromBuffer(std::vector<char>& mc_code,
                          const std::string& section_name,
                          const std::string& /*argname*/)
  {
    // For dpu
    auto instr_ptr = reinterpret_cast<const uint32_t*>(mc_code.data());
    size_t inst_word_size = mc_code.size()/4;
    size_t pc = 0;

    while (pc < inst_word_size) {
      uint32_t opcode = (instr_ptr[pc] & 0xFF000000) >> 24;
      switch(opcode) {
        case OP_WRITESHIMBD: patch_shimbd(instr_ptr, pc, section_name);
          pc += OP_WRITESHIMBD_SIZE;
          break;
        case OP_WRITEBD:
        {
          uint8_t row = (instr_ptr[pc] & 0x0000FF00) >> 8;
          if (row == 0)
          {
            patch_shimbd(instr_ptr, pc, section_name);
            pc += OP_WRITEBD_SIZE_9;
          }
          else if (row == 1)
            pc += OP_WRITEBD_SIZE_9;
          else
            pc += OP_WRITEBD_SIZE_7;
          break;
        }
        case OP_NOOP: pc += OP_NOOP_SIZE;
          break;
        case OP_WRITE32: pc += OP_WRITE32_SIZE;
          break;
        case OP_WRITEBD_EXTEND_AIETILE: pc += OP_WRITEBD_EXTEND_AIETILE_SIZE;
          break;
        case OP_WRITE32_EXTEND_GENERAL: pc += OP_WRITE32_EXTEND_GENERAL_SIZE;
          break;
        case OP_WRITEBD_EXTEND_SHIMTILE: pc += OP_WRITEBD_EXTEND_SHIMTILE_SIZE;
          break;
        case OP_WRITEBD_EXTEND_MEMTILE: pc += OP_WRITEBD_EXTEND_MEMTILE_SIZE;
          break;
        case OP_WRITE32_EXTEND_DIFFBD: pc += OP_WRITE32_EXTEND_DIFFBD_SIZE;
          break;
        case OP_WRITEBD_EXTEND_SAMEBD_MEMTILE: pc += OP_WRITEBD_EXTEND_SAMEBD_MEMTILE_SIZE;
          break;
        case OP_DUMPDDR: pc += OP_DUMPDDR_SIZE;  //TODO: get pc based on hw/simnow
          break;
        case OP_WRITEMEMBD: pc += OP_WRITEMEMBD_SIZE;
          break;
        case OP_WRITE32_RTP: pc += OP_WRITE32_RTP_SIZE;
          break;
        case OP_READ32: pc += OP_READ32_SIZE;
          break;
        case OP_READ32_POLL: pc += OP_READ32_POLL_SIZE;
          break;
        case OP_SYNC: pc += OP_SYNC_SIZE;
          break;
        case OP_MERGESYNC: pc += OP_MERGESYNC_SIZE;
          break;
        case OP_DUMP_REGISTER:
        { pc += 1;
          uint32_t count = instr_ptr[pc] & 0x00FFFFFF;
          uint32_t total = count << 1;
          pc += total;
          break;
        }
        case OP_RECORD_TIMESTAMP: pc += OP_RECORD_TIMESTAMP_SIZE;
          break;
        default:
          throw error(error::error_code::invalid_asm, "Invalid dpu opcode: " + std::to_string(opcode) + " !!!");
      }
    }
    return 0;
  }
}
