// Copyright 2018 ETH Zurich and University of Bologna.
// Copyright and related rights are licensed under the Solderpad Hardware
// License, Version 0.51 (the "License"); you may not use this file except in
// compliance with the License.  You may obtain a copy of the License at
// http://solderpad.org/licenses/SHL-0.51. Unless required by applicable law
// or agreed to in writing, software, hardware and materials distributed under
// this License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.
//
// Engineer:       Francesco Conti - f.conti@unibo.it
//
// Additional contributions by:
//                 Markus Wegmann - markus.wegmann@technokrat.ch
//
// Design Name:    RISC-V register file
// Project Name:   zero-riscy
// Language:       SystemVerilog
//
// Description:    Register file with 31 or 15x 32 bit wide registers.
//                 Register 0 is fixed to 0. This register file is based on
//                 flip flops.
//

module ariane_regfile #(
    parameter config_pkg::cva6_cfg_t CVA6Cfg       = config_pkg::cva6_cfg_empty,
    parameter int unsigned           DATA_WIDTH    = 32,
    parameter int unsigned           NR_READ_PORTS = 2,
    parameter bit                    ZERO_REG_ZERO = 0,
    parameter int unsigned           PHYSICAL_REGS = 64
) (
    // clock and reset
    input  logic                                             clk_i,
    input  logic                                             rst_ni,
    // disable clock gates for testing
    input  logic                                             test_en_i,
    // register window configuration: [31:16] = size, [15:0] = base
    input  logic [CVA6Cfg.XLEN-1:0]                          window_config_i,
    // read port
    input  logic [        NR_READ_PORTS-1:0][           4:0] raddr_i,
    output logic [        NR_READ_PORTS-1:0][DATA_WIDTH-1:0] rdata_o,
    // write port
    input  logic [CVA6Cfg.NrCommitPorts-1:0][           4:0] waddr_i,
    input  logic [CVA6Cfg.NrCommitPorts-1:0][DATA_WIDTH-1:0] wdata_i,
    input  logic [CVA6Cfg.NrCommitPorts-1:0]                 we_i
);

  localparam int unsigned ADDR_WIDTH = (PHYSICAL_REGS > 1) ? $clog2(PHYSICAL_REGS) : 1;
  localparam int unsigned NUM_WORDS = PHYSICAL_REGS;

  logic [            NUM_WORDS-1:0][DATA_WIDTH-1:0] mem;
  logic [CVA6Cfg.NrCommitPorts-1:0][ NUM_WORDS-1:0] we_dec;
  logic [15:0] window_base, window_size;
  logic [NR_READ_PORTS-1:0][ADDR_WIDTH-1:0] phys_raddr;
  logic [NR_READ_PORTS-1:0] read_in_window;
  logic [CVA6Cfg.NrCommitPorts-1:0][ADDR_WIDTH-1:0] phys_waddr;
  logic [CVA6Cfg.NrCommitPorts-1:0] write_in_window;

  assign window_base = window_config_i[15:0];
  assign window_size = window_config_i[31:16];

  always_comb begin : window_translation
    for (int unsigned i = 0; i < NR_READ_PORTS; i++) begin
      read_in_window[i] = 1'b0;
      phys_raddr[i]     = '0;
      if (raddr_i[i] < window_size && (window_base + raddr_i[i]) < PHYSICAL_REGS) begin
        read_in_window[i] = 1'b1;
        phys_raddr[i]     = ADDR_WIDTH'(window_base + raddr_i[i]);
      end
    end
    for (int unsigned i = 0; i < CVA6Cfg.NrCommitPorts; i++) begin
      write_in_window[i] = 1'b0;
      phys_waddr[i]      = '0;
      if (waddr_i[i] < window_size && (window_base + waddr_i[i]) < PHYSICAL_REGS) begin
        write_in_window[i] = 1'b1;
        phys_waddr[i]      = ADDR_WIDTH'(window_base + waddr_i[i]);
      end
    end
  end


  always_comb begin : we_decoder
    for (int unsigned j = 0; j < CVA6Cfg.NrCommitPorts; j++) begin
      for (int unsigned i = 0; i < NUM_WORDS; i++) begin
        if (write_in_window[j] && (phys_waddr[j] == i)) we_dec[j][i] = we_i[j];
        else we_dec[j][i] = 1'b0;
      end
    end
  end

  // loop from 1 to NUM_WORDS-1 as R0 is nil
  always_ff @(posedge clk_i, negedge rst_ni) begin : register_write_behavioral
    if (~rst_ni) begin
      mem <= '{default: '0};
    end else begin
      for (int unsigned j = 0; j < CVA6Cfg.NrCommitPorts; j++) begin
        for (int unsigned i = 0; i < NUM_WORDS; i++) begin
          if (we_dec[j][i]) begin
            mem[i] <= wdata_i[j];
          end
        end
        if (ZERO_REG_ZERO) begin
          mem[0] <= '0;
        end
      end
    end
  end

  for (genvar i = 0; i < NR_READ_PORTS; i++) begin
    always_comb begin
      rdata_o[i] = '0;
      if (read_in_window[i]) begin
        rdata_o[i] = mem[phys_raddr[i]];
      end
      if (ZERO_REG_ZERO && (raddr_i[i] == '0)) begin
        rdata_o[i] = '0;
      end
    end
  end

endmodule
