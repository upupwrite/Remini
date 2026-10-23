#ifndef BLOCKSUPPORT_H
#define BLOCKSUPPORT_H

// Common positional contract for Markdown constructs that span a block or a
// range within one. Code blocks expose block numbers; inline fragments expose
// character offsets. Consumers that need a span can handle both uniformly.
class BlockSupport
{
public:
    virtual ~BlockSupport() = default;
    virtual int getStart() const = 0;
    virtual int getEnd() const = 0;
};

#endif  // BLOCKSUPPORT_H
