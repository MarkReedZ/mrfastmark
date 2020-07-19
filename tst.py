
import mistletoe
import mistune
from paka import cmark

mis = mistune.Markdown()

import mrfastmark as fm
import misaka
#misaka = misaka.Markdown(misaka.SaferHtmlRenderer(flags=('hard-wrap',)), ('autolink','space-headers','fenced-code','superscript') )
misaka = misaka.Markdown(misaka.SaferHtmlRenderer(flags=()), ('autolink','space-headers','fenced-code','superscript') )

txt="``` no closing"
txt="""
*hello* 

\*hello\* 

*\*hello\** 

\**hello*\* 

**hello \** world**
"""
txt="&#X22; &#XD06; &#xcab; &; &tst &&lt;"
txt="&#X22; &#; &tst &&lt;"

print( "Mine:\n\n",fm.render(txt) )
print( "Misaka:\n\n",misaka(txt) )
print( "mistletoe:\n\n",mistletoe.markdown(txt) )
print( "mistune:\n\n",mis(txt) )
print( "cmark:\n\n", cmark.to_html(txt) )

exit()

#from html2text import html2text
#print(html2text( fm.render(txt) ))
#print(html2text( mis(txt) ))
