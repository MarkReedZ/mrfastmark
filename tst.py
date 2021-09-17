
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
[link text](http://dev.nodeca.com)
"""
txt="""
-one
- two
-three"""

xt="""


"""

#o = open("tst.txt","r")
#txt = o.read()
#o.close()

#txt="&#X22; &#XD06; &#xcab; &; &tst &&lt;"
#txt="&#X22; &#; &tst &&lt;"

print( "cmark:\n\n", cmark.to_html(txt) )
print( "Mine:\n\n",fm.render(txt) )
print( "Misaka:\n\n",misaka(txt) )
print( "mistletoe:\n\n",mistletoe.markdown(txt) )
print( "mistune:\n\n",mis(txt) )


#from html2text import html2text
#print(html2text( fm.render(txt) ))
#print(html2text( mis(txt) ))

import random 
for x in range(100):
  s = ""
  for z in range(random.randrange(0,2000)):
    s += chr(random.randrange(0,0x10FFFF))
  try:
    fm.render(s)
  except:
    pass


