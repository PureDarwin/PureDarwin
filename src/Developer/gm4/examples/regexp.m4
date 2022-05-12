traceon(`regexp')dnl
regexp(`hej med dig', `.*', `>>\0<<')
regexp(`hej med dig', `\w*', `>>\0<<')
regexp(`hej med dig', `.+', `>>\0<<')
regexp(`hej med dig', `m\w+', `>>\0<<')
regexp(`hej med dig', `m\(.*\)', `>>\0<< >>\1<<')

regexp(`hej med dig', `.*')
regexp(`hej med dig', `\w*')
regexp(`hej med dig', `.+')
regexp(`hej med dig', `m\w+')
regexp(`hej med dig', `m\(.*\)')
