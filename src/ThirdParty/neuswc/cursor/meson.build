tool = executable('convert_cont', 'convert_font.c', native: true)

cursor = custom_target('cursor_data_h',
  input: 'cursor.pcf',
  output: 'cursor_data.h',
  command: [tool, '@INPUT@', '@OUTPUT@']
)
