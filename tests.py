# coding=UTF-8

import mistletoe
import mrfastmark as fm
import inspect
import mistune
mis = mistune.Markdown()


def raises( o, f, exc,details ):
  try:
    f(o)
    if len(o) > 100: o = o[:100]
    print("ERROR ", o, " didn't raise exception")
  except Exception as e:
    if len(o) > 100: o = o[:100]
    if type(e) != exc:
      print("ERROR",o," rose wrong exception",type(e),e)
    if str(e) != details:
      print("ERROR",o," rose wrong exception details actual vs expected:\n",e,"\n",details)

def pbuf( b ):
  s = ""
  for x in b:
    s += hex(ord(x))  + " "
  print(s)

def eq( a, b, o ):
  if a != b:
    cf = inspect.currentframe()
    print( "ERROR Line", cf.f_back.f_lineno, a, "!=", b )
    #print( "mistletoe:", mistletoe.markdown(o) )
    print( "mistune:", mis(o) )
    pbuf(a)
    pbuf(b)
    #pbuf(mistletoe.markdown(o))
    pbuf(mis(o))
    return -1
  return 0


print("Running tests...")

tsts =[
  [ "><&\"", """<blockquote>\x0a<p>&lt;&amp;&quot;</p>\x0a</blockquote>""" ],
  [ "# Test\x0aLine ends on end of string", "<h1>Test</h1>\x0a<p>Line ends on end of string</p>\x0a"],
  [ "This is some [markdown rendered by mrfastmark](https://github.com/MarkReedZ/mrfastmark)\x0a\x0aIs it working?", 
    "<p>This is some <a href=\"https://github.com/MarkReedZ/mrfastmark\">markdown rendered by mrfastmark</a></p>\x0a<p>Is it working?</p>\x0a" ],

  # Auto link
  [ "This is a link http://github.com/MarkReedZ/mrfastmark", "<p>This is a link <a href=\"http://github.com/MarkReedZ/mrfastmark\">http://github.com/MarkReedZ/mrfastmark</a></p>\x0a" ],
  [ "This is a link https://github.com/MarkReedZ/mrfastmark", "<p>This is a link <a href=\"https://github.com/MarkReedZ/mrfastmark\">https://github.com/MarkReedZ/mrfastmark</a></p>\x0a" ],
  [ "This is a link http://github.com/MarkReedZ/mrfastmark\x0a\x0a", "<p>This is a link <a href=\"http://github.com/MarkReedZ/mrfastmark\">http://github.com/MarkReedZ/mrfastmark</a></p>\x0a" ],
  [ "This is a link https://github.com/MarkReedZ/mrfastmark\x0a\x0a", "<p>This is a link <a href=\"https://github.com/MarkReedZ/mrfastmark\">https://github.com/MarkReedZ/mrfastmark</a></p>\x0a" ],

  # Double break after p?
  [ "<&\"z##", """<p>&lt;&amp;&quot;z##</p>\x0a"""],
  [ "**Bold**right?","<p><strong>Bold</strong>right?</p>\x0a"], 
  [ ">**Bold**\n>\n> still quoted","<blockquote>\x0a<p><strong>Bold</strong></p>\x0a<p> still quoted</p>\x0a</blockquote>" ],
  [ ">test\nalso quoted","<blockquote>\x0a<p>test\x0aalso quoted</p>\x0a</blockquote>" ],
  [ ">test\nalso quoted\n\nNot","<blockquote>\x0a<p>test\x0aalso quoted</p>\x0a</blockquote>\x0a<p>Not</p>\x0a" ],

  # Lists
  [ "1. one\x0a2. two\x0a3. three", "<ol>\x0a<li><p>one</p>\x0a</li>\x0a<li><p>two</p>\x0a</li>\x0a<li><p>three</p>\x0a</li>\x0a</ol>\x0a" ],
  [ "- one\x0a- two\x0a- three", "<ul>\x0a<li><p>one</p>\x0a</li>\x0a<li><p>two</p>\x0a</li>\x0a<li><p>three</p>\x0a</li>\x0a</ul>\x0a" ],
  # TODO more

  # Code block
  [ "```\x0a- one\x0a- two\x0a- three\x0a```", "<pre><code>- one\x0a- two\x0a- three\x0a</code></pre>\x0a" ],
  [ "Albert\x0a```\x0a- one\x0a- two\x0a- three\x0a```Einstein", "<p>Albert</p>\x0a<pre><code>- one\x0a- two\x0a- three\x0a</code></pre>\x0a<p>Einstein</p>\x0a"], # cm spec ignores Einstein
  [ "foo\x0a```\x0abar\x0a```\x0abaz","<p>foo</p>\x0a<pre><code>bar\x0a</code></pre>\x0a<p>baz</p>\x0a" ],
  [ "~~~\x0a- one\x0a- two\x0a- three\x0a~~~", "<pre><code>- one\x0a- two\x0a- three\x0a</code></pre>\x0a" ],
  [ "Albert\x0a~~~\x0a- one\x0a- two\x0a- three\x0a~~~Einstein", "<p>Albert</p>\x0a<pre><code>- one\x0a- two\x0a- three\x0a</code></pre>\x0a<p>Einstein</p>\x0a"], # cm spec ignores Einstein
  [ "foo\x0a~~~\x0abar\x0a~~~\x0abaz","<p>foo</p>\x0a<pre><code>bar\x0a</code></pre>\x0a<p>baz</p>\x0a" ],

]

for t in tsts:
  eq( fm.render(t[0]), t[1], t[0] )

# TODO strip html tags test, need to set option


print("Testing Exceptions..")
#raises( "NaNd",         j.loads, ValueError, "Expecting 'NaN' at pos 0" )

print("Done")
