from ctrlcode.common.util import parse_num_arg
from ctrlcode.common.section import Section
from ctrlcode.ops.serializer.op_serializer import OpSerializer

class WordOpSerializer(OpSerializer):
    def __init__(self, op, args, state):
        assert len(args) == 1
        super().__init__(op, args, state)
        self.val = parse_num_arg(args[0], self.state)

    def size(self):
        return 4

    def align(self):
        return 4

    def serialize(self, writer, col, page, symbols):
        assert (self.state.section == Section.DATA), "Words can only be used in DATA section"
        writer.write_words([self.val], Section.DATA, col, page)
