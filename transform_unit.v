`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 05/24/2023 10:55:35 AM
// Design Name: 
// Module Name: transform_unit
// Project Name: 
// Target Devices: 
// Tool Versions: 
// Description: 
// 
// Dependencies: 
// 
// Revision:
// Revision 0.01 - File Created
// Additional Comments:
// 
//////////////////////////////////////////////////////////////////////////////////


module transform_unit(
   input                   valid_i,
   output reg              ready_o,
   input          [ 3:0]   wstrb_i,
   input          [31:0]   addr_i,
   input          [31:0]   wdata_i,
   output reg     [31:0]   rdata_o,
   input                   clk_i,
   input                   resetn_i
   
   );
   
   localparam BASE_ADDRESS = 32'h4000_0000;
   /*
   BA + 0 -> BA + 63 (INPUTS)
   BA + 64 -> BA + 127 (OUTPUTS)
   BA + 128 (CONTROL)
   
   */
   (* mark_debug = "true" *) reg         done; 
   reg signed [7:0]   input_regs	   [0:63];
   reg signed [7:0]   output_regs	[0:63];
   
   always @(posedge clk_i) begin
      if (resetn_i == 0) begin
         ready_o <= 0;
         rdata_o <= 0;
      end else begin
         if (valid_i == 1'b1) begin
            if (ready_o == 1'b1) begin
               ready_o <= 0;
               rdata_o <= 0;
            end else begin
               if (wstrb_i == 0) begin
                  if (done == 1'b1) begin
                     ready_o <= 1;
                     if (addr_i[6] == 1'b0) begin
                        rdata_o <= {input_regs[addr_i[5:0] + 3], input_regs[addr_i[5:0] + 2], input_regs[addr_i[5:0] + 1], input_regs[addr_i[5:0] + 0]};
                     end else begin
                        rdata_o <= {output_regs[addr_i[5:0] + 3], output_regs[addr_i[5:0] + 2], output_regs[addr_i[5:0] + 1], output_regs[addr_i[5:0] + 0]};
                     end
                  end else begin // if not done
                     ready_o <= ready_o;
                     rdata_o <= rdata_o;
                  end
               end else begin
                  ready_o <= 1;
                  if (addr_i[6] == 1'b0) begin
                     input_regs[addr_i[5:0] + 3] <= wstrb_i[3] ? wdata_i[31:24] : input_regs[addr_i[5:0] + 3];
                     input_regs[addr_i[5:0] + 2] <= wstrb_i[2] ? wdata_i[23:16] : input_regs[addr_i[5:0] + 2];
                     input_regs[addr_i[5:0] + 1] <= wstrb_i[1] ? wdata_i[15:8]  : input_regs[addr_i[5:0] + 1];
                     input_regs[addr_i[5:0] + 0] <= wstrb_i[0] ? wdata_i[7:0]   : input_regs[addr_i[5:0] + 0];
                  end
               end
            end
         end else begin
            ready_o <= ready_o;
            rdata_o <= rdata_o;
         end
      end
   end 

   wire signed [7:0] weights[0:7];
   wire        [2:0] select_bits[0:55];


   assign weights[0] = -8'sd1;
   assign weights[1] = 8'sd2;
   assign weights[2] = 8'sd6;
   assign weights[3] = 8'sd2;
   assign weights[4] = -8'sd1;

   assign weights[5] = -8'sd1;
   assign weights[6] = 8'sd2;
   assign weights[7] = -8'sd1;

   reg       general_state;
   reg [3:0] level;
   reg [1:0] mult_index;
   reg [2:0] lb_state; // register for line block state machine
   reg       vertical;

   localparam LB_IDLE      = 3'b000;
   localparam LB_LOAD_IN   = 3'b001;
   localparam LB_SELECT    = 3'b010;
   localparam LB_MULTIPLY  = 3'b011;
   localparam LB_COMPUTE   = 3'b100;
   localparam LB_TRANSPOSE = 3'b101;
   localparam LB_LOAD_OUT  = 3'b110;

   localparam C_IDLE    = 1'b0;
   localparam C_WORKING = 1'b1;

   always @(posedge clk_i) begin : control_machine
      if (resetn_i == 1'b0) begin
         general_state <= 0;
         level <= 0;
         mult_index <= 0;
         lb_state <= LB_IDLE;
         done <= 1;
         vertical <= 0;
      end else begin
         case(general_state)
            C_IDLE: begin
               if (valid_i == 1'b1 && addr_i == BASE_ADDRESS + 60 && ready_o == 1'b1) begin
                  general_state <= C_WORKING;
                  done <= 0;
               end else begin
                  general_state <= general_state;
                  done <= done;
               end
            end
            C_WORKING: begin
               case(lb_state)
                  LB_IDLE: begin
                     lb_state <= LB_LOAD_IN;
                  end
                  LB_LOAD_IN: begin
                     level <= 4'b1000;
                     mult_index <= 2'b00;
                     vertical <= 0;
                     lb_state <= LB_SELECT;
                  end
                  LB_SELECT: begin
                     lb_state <= LB_MULTIPLY;
                  end
                  LB_MULTIPLY: begin
                     lb_state <= LB_COMPUTE;
                  end
                  LB_COMPUTE: begin
                     case(level)
                        4'b1000: begin
                           if (mult_index == 2'b11) begin
                              if (vertical == 1'b1) begin
                                 level <= 4'b0100;
                                 mult_index <= 2'b00;
                                 vertical <= 0;
                                 lb_state <= LB_TRANSPOSE;
                              end else begin
                                 level <= level;
                                 mult_index <= 2'b00;
                                 vertical <= 1;
                                 lb_state <= LB_TRANSPOSE;
                              end
                           end else begin
                              level <= level;
                              mult_index <= mult_index + 1;
                              vertical <= vertical;
                              lb_state <= LB_SELECT;
                           end
                        end
                        4'b0100: begin
                           if (mult_index == 2'b01) begin
                              if (vertical == 1'b1) begin
                                 level <= 4'b0010;
                                 mult_index <= 0;
                                 vertical <= 0;
                                 lb_state <= LB_TRANSPOSE;
                              end else begin
                                 level <= level;
                                 mult_index <= 0;
                                 vertical <= 1;
                                 lb_state <= LB_TRANSPOSE;
                              end
                           end else begin
                              level <= level;
                              mult_index <= mult_index + 1;
                              vertical <= vertical;
                              lb_state <= LB_SELECT;
                           end
                        end
                        4'b0010: begin
                           if (vertical == 1'b1) begin // if we're fully done
                              level <= level;
                              mult_index <= mult_index;
                              vertical <= 0;
                              lb_state <= LB_LOAD_OUT;
                           end else begin
                              level <= level;
                              mult_index <= mult_index;
                              vertical <= 1;
                              lb_state <= LB_TRANSPOSE;
                           end
                        end
                     endcase
                  end
                  LB_TRANSPOSE: begin
                     lb_state <= LB_SELECT;
                  end
                  LB_LOAD_OUT: begin
                     general_state <= C_IDLE;
                     lb_state <= LB_IDLE;
                     done <= 1;
                  end
               endcase
            end
         endcase
      end
   end

   reg signed [7:0] line_inputs[0:63];
   reg signed [7:0] line_outputs[0:63];


   genvar i;
   genvar w;
   genvar j;
   genvar k;
   generate
      for(i = 0; i < 8; i = i + 1) begin // each line block
         reg signed [7:0] sel_inputs 	[0:7]; 
         reg signed [15:0] results		[0:7]; 

         reg signed [15:0] temp_low;
         reg signed [15:0] temp_high;

         reg signed [7:0] low_sum; // comb
         reg signed [7:0] high_sum; // comb
         
            // convolution loops
            /*
         for(j = 0; j < 5; j = j + 1) begin // low-pass
            always @(*) begin
            end
         end
         

         for(j = 5; j < 8; j = j + 1) begin // high-pass
            always @(*) begin
            end
         end*/

         always @(*) begin
            // summation
            temp_high = results[5] + results[6] + results[7];
            temp_low = results[0] + results[1] + results[2] + results[3] + results[4];
            // shift (division + floor)
            high_sum[7:0] = temp_high[8:1];
            low_sum[7:0] = temp_low[10:3];
         end

         for (j = 0; j < 8; j = j + 1) begin
            always @(posedge clk_i) begin
               if (resetn_i == 1'b0) begin
                  line_inputs[j*8 + i] <= 0;
                  line_outputs[i*8 + j] <= 0;
               end else begin
                  case(lb_state)
                     LB_IDLE: begin
                        // Everything is held
                        line_inputs[j*8 + i] <= line_inputs[j*8 + i];
                        line_outputs[i*8 + j] <= line_outputs[i*8 + j];
                     end
                     LB_LOAD_IN: begin
                        // load each input from outside regs
                        line_inputs[j*8 + i] <= input_regs[j*8 + i];
                        line_outputs[i*8 + j] <= line_outputs[i*8 + j];
                     end
                     LB_SELECT: begin
                        sel_inputs[j] <= line_inputs[i*8 + select_bits[(level - mult_index  - 2) * 8 + j]];
                     end
                     LB_MULTIPLY: begin
                        results[j] <= sel_inputs[j] * weights[j];
                     end
                     LB_COMPUTE: begin
                        // Inputs are held
                        line_inputs[j*8 + i] <= line_inputs[j*8 + i];
                        // Relevant outputs are written
                        //line_outputs[i*8 + j] <= (j == (level - abs(level - 2*mult_index))/2 && i < level) ? (j < i/2 ? low_sum : high_sum) : line_inputs[i*8 + j];
                        //line_outputs[i*8 + j] <= (j < level/2) ? ((j == (level - abs(level - 2*mult_index))/2 && i < level) ? (low_sum):(line_inputs[i*8 + j])):( (j - level/2 == (level - abs(level - 2*mult_index))/2)? (high_sum):(line_inputs[i*8 + j]));
                        /*if (j < level/2) begin
                           if (j == (level/2 - abs(level/2 - mult_index)) && i < level) begin
                              line_outputs[i*8 + j] <= low_sum;
                           end else begin
                              line_outputs[i*8 + j] <= line_inputs[i*8 + j];
                           end
                        end else begin
                           if ((j == (level - abs(level/2 - mult_index))) && i < level) begin
                              line_outputs[i*8 + j] <= high_sum;
                           end else begin
                              line_outputs[i*8 + j] <= line_inputs[i*8 + j];
                           end
                        end*/
                        if (j < level && i < level) begin
                           if (j == mult_index) begin
                              line_outputs[i*8 + j] <= low_sum;
                           end else if (j == level/2 + mult_index) begin
                              line_outputs[i*8 + j] <= high_sum;
                           end else begin
                              line_outputs[i*8 + j] <= line_outputs[i*8 + j];
                           end
                        end else begin
                           line_outputs[i*8 + j] <= line_inputs[i*8 + j];
                        end
                     end
                     LB_TRANSPOSE: begin
                        line_inputs[j*8 + i] <= line_outputs[i*8 + j];
                        line_outputs[i*8 + j] <= line_outputs[i*8 + j];
                     end
                     LB_LOAD_OUT: begin
                        // load each output to outside regs
                        line_inputs[j*8 + i] <= line_inputs[j*8 + i];
                        output_regs[i*8 + j] <= line_outputs[i*8 + j];
                     end
                  endcase
               end
            end
         end
         
      end
      
   endgenerate
      
   function [31:0] abs;
      input [31:0] number;
      begin
            if (number < 0)
               abs = -number;
            else
               abs = number;
      end
   endfunction
   assign select_bits[55] = 3'd2;
   assign select_bits[54] = 3'd1;
   assign select_bits[53] = 3'd0;
   assign select_bits[52] = 3'd2;
   assign select_bits[51] = 3'd1;
   assign select_bits[50] = 3'd0;
   assign select_bits[49] = 3'd1;
   assign select_bits[48] = 3'd2;
   assign select_bits[47] = 3'd4;
   assign select_bits[46] = 3'd3;
   assign select_bits[45] = 3'd2;
   assign select_bits[44] = 3'd4;
   assign select_bits[43] = 3'd3;
   assign select_bits[42] = 3'd2;
   assign select_bits[41] = 3'd1;
   assign select_bits[40] = 3'd0;
   assign select_bits[39] = 3'd6;
   assign select_bits[38] = 3'd5;
   assign select_bits[37] = 3'd4;
   assign select_bits[36] = 3'd6;
   assign select_bits[35] = 3'd5;
   assign select_bits[34] = 3'd4;
   assign select_bits[33] = 3'd3;
   assign select_bits[32] = 3'd2;
   assign select_bits[31] = 3'd6;
   assign select_bits[30] = 3'd7;
   assign select_bits[29] = 3'd6;
   assign select_bits[28] = 3'd6;
   assign select_bits[27] = 3'd7;
   assign select_bits[26] = 3'd6;
   assign select_bits[25] = 3'd5;
   assign select_bits[24] = 3'd4;
   assign select_bits[23] = 3'd2;
   assign select_bits[22] = 3'd1;
   assign select_bits[21] = 3'd0;
   assign select_bits[20] = 3'd2;
   assign select_bits[19] = 3'd1;
   assign select_bits[18] = 3'd0;
   assign select_bits[17] = 3'd1;
   assign select_bits[16] = 3'd2;
   assign select_bits[15] = 3'd2;
   assign select_bits[14] = 3'd3;
   assign select_bits[13] = 3'd2;
   assign select_bits[12] = 3'd2;
   assign select_bits[11] = 3'd3;
   assign select_bits[10] = 3'd2;
   assign select_bits[9] = 3'd1;
   assign select_bits[8] = 3'd0;
   assign select_bits[7] = 3'd0;
   assign select_bits[6] = 3'd1;
   assign select_bits[5] = 3'd0;
   assign select_bits[4] = 3'd0;
   assign select_bits[3] = 3'd1;
   assign select_bits[2] = 3'd0;
   assign select_bits[1] = 3'd1;
   assign select_bits[0] = 3'd0;
endmodule


/*
NOTES FOR TOMORROW:
- As we progress levels, some lines need to be skipped;
- implement shifts


*/