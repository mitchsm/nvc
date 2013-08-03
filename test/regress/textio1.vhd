entity textio1 is
end entity;

use std.textio.all;

architecture test of textio1 is
begin

    process is
        variable l : line;
    begin
        write(l, string'("hello, world"));
        writeline(output, l);
        assert l'length = 0;
        write(l, string'("one"));
        write(l, ' ');
        write(l, string'("two"));
        writeline(output, l);
        wait;
    end process;

end architecture;