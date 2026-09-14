#pragma once

#include <cstdint>
#include <format>


struct alignas(64) MarketUpdatePOD {
    uint64_t timestamp_ns;  // Nanosecond Unix timestamp
    uint32_t sequence_num;  // Packet sequence for gap detection
    char     symbol[8];     // Fixed-size char array (no dynamic std::string!)
    uint32_t price_fixed;   // Fixed-point price (e.g., USD cents or scaled by 10,000)
    uint32_t quantity;      // Order size or volume
    char     side;          // 'B' for Buy, 'A' for Ask/Sell
    uint8_t  update_type;   // 1 = New, 2 = Modify, 3 = Cancel
};

static_assert( sizeof(MarketUpdatePOD) == 64 );


template <>
struct std::formatter<MarketUpdatePOD> {
    constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin(); 
    }

    auto format(const MarketUpdatePOD& msg, std::format_context& ctx) const {
        // Convert the char array to a clean string_view safely
        std::string_view sym(msg.symbol, 8);
        
        // Use format_to to pipe the fields sequentially into the output context buffer
        return std::format_to(ctx.out(),
            "MarketUpdate:\n"
            "  Timestamp: {} ns\n"
            "  Seq Num:   {}\n"
            "  Symbol:    {}\n"
            "  Price:     {}\n"
            "  Quantity:  {}\n"
            "  Side:      {}\n"
            "  Type:      {}",
            msg.timestamp_ns,
            msg.sequence_num,
            sym,
            msg.price_fixed,
            msg.quantity,
            msg.side,
            static_cast<int>(msg.update_type) // Cast uint8_t so it prints as a number, not a char
        );
    }
};

