# coding=UTF-8

import mistletoe
import mrfastmark as fm
import inspect, random, string
import mistune
mis = mistune.Markdown()


def raises( o, f, exc,details ):
  try:
    f(o)
    cf = inspect.currentframe()
    print("ERROR line",cf.f_back.f_lineno,"didn't raise exception",str(exc))
  except Exception as e:
    cf = inspect.currentframe()
    if type(e) != exc:
      print("ERROR line",cf.f_back.f_lineno,"rose the wrong exception",str(e))
    if str(e) != details:
      print("ERROR line",cf.f_back.f_lineno,"raised an exception with the wrong details\nAct: ",str(e),"\nExp: ",details)

def pbuf( b ):
  s = ""
  for x in b:
    s += hex(ord(x))  + " "
  print(s)

def eq( a, b, o ):
  if a != b:
    cf = inspect.currentframe()
    print( "ERROR Line", cf.f_back.f_lineno, a, "!=", b )
    print( "mistletoe:", mistletoe.markdown(o) )
    print( "mistune:", mis(o) )
    pbuf(a)
    pbuf(b)
    #pbuf(mistletoe.markdown(o))
    pbuf(mis(o))
    exit()
    return -1
  return 0


print("Running tests...")

tsts =[
  [ "><&\"", """<blockquote>\x0a<p>&lt;&amp;&quot;</p>\x0a</blockquote>""" ],
  [ "# Test\x0aLine ends on end of string", "<h1>Test</h1>\x0a<p>Line ends on end of string</p>\x0a"],
  [ "This is some [markdown rendered by mrfastmark](https://github.com/MarkReedZ/mrfastmark)\x0a\x0aIs it working?", 
    "<p>This is some <a href=\"https://github.com/MarkReedZ/mrfastmark\">markdown rendered by mrfastmark</a></p>\x0a<p>Is it working?</p>\x0a" ],

  # Headers
  [ "##5 dogs","<p>##5 dogs</p>\x0a"], # must have a space after the header #.  
  #[ "#         spaces         ","<h1>spaces</h1>\x0a"], 
  [ "## head","<h2>head</h2>\x0a"], 
  [ "### head","<h3>head</h3>\x0a"], 
  [ "#### head","<h4>head</h4>\x0a"], 
  [ "##### head","<h5>head</h5>\x0a"], 
  [ "###### head","<h6>head</h6>\x0a"], 
  [ "#→Foo","<p>#→Foo</p>\x0a"], # must have a space after the header #.  

  # Auto link
  [ "This is a link http://github.com/MarkReedZ/mrfastmark", "<p>This is a link <a href=\"http://github.com/MarkReedZ/mrfastmark\">http://github.com/MarkReedZ/mrfastmark</a></p>\x0a" ],
  [ "This is a link https://github.com/MarkReedZ/mrfastmark", "<p>This is a link <a href=\"https://github.com/MarkReedZ/mrfastmark\">https://github.com/MarkReedZ/mrfastmark</a></p>\x0a" ],
  [ "This is a link http://github.com/MarkReedZ/mrfastmark\x0a\x0a", "<p>This is a link <a href=\"http://github.com/MarkReedZ/mrfastmark\">http://github.com/MarkReedZ/mrfastmark</a></p>\x0a" ],
  [ "This is a link https://github.com/MarkReedZ/mrfastmark\x0a\x0a", "<p>This is a link <a href=\"https://github.com/MarkReedZ/mrfastmark\">https://github.com/MarkReedZ/mrfastmark</a></p>\x0a" ],
  [ "irc://foo.bar:2233/baz\x0a", "<p>irc://foo.bar:2233/baz</p>\x0a" ],

  [ "[link text](http://dev.nodeca.com)", '<p><a href="http://dev.nodeca.com">link text</a></p>\x0a' ],


  # Double break after p?
  [ "<&\"z##", """<p>&lt;&amp;&quot;z##</p>\x0a"""],
  [ "**Bold**right?","<p><strong>Bold</strong>right?</p>\x0a"], 
  [ ">**Bold**\n>\n> still quoted","<blockquote>\x0a<p><strong>Bold</strong></p>\x0a<p> still quoted</p>\x0a</blockquote>" ],
  [ ">test\nalso quoted","<blockquote>\x0a<p>test\x0aalso quoted</p>\x0a</blockquote>" ],
  [ ">test\nalso quoted\n\nNot","<blockquote>\x0a<p>test\x0aalso quoted</p>\x0a</blockquote>\x0a<p>Not</p>\x0a" ],

  # Lists
  [ "1. one\x0a2. two\x0a3. three", "<ol>\x0a<li> one</li>\x0a<li> two</li>\x0a<li> three</li>\x0a</ol>\x0a" ],
  [ "- one\x0a- two\x0a- three", "<ul>\x0a<li> one</li>\x0a<li> two</li>\x0a<li> three</li></ul>" ],
  [ "-notalist\x0a- two\x0a-three", "<p>-notalist</p>\x0a<ul>\x0a<li> two\x0a-three</li>\x0a</ul>\x0a" ],

  # TODO more

  # Code block
  #[ "``` no closer", "foo" ],  # What to do here? Close it on end of input?
  [ "```\x0a- one\x0a- two\x0a- three\x0a```", "<pre><code>\x0a- one\x0a- two\x0a- three\x0a</code></pre>\x0a" ],
  [ "Albert\x0a```\x0a- one\x0a- two\x0a- three\x0a```Einstein", "<p>Albert</p>\x0a<pre><code>\x0a- one\x0a- two\x0a- three\x0a</code></pre>\x0a<p>Einstein</p>\x0a"], # cm spec ignores Einstein
  [ "foo\x0a```\x0abar\x0a```\x0abaz","<p>foo</p>\x0a<pre><code>\x0abar\x0a</code></pre>\x0a<p>baz</p>\x0a" ],
  [ "~~~\x0a- one\x0a- two\x0a- three\x0a~~~", "<pre><code>- one\x0a- two\x0a- three\x0a</code></pre>\x0a" ],
  [ "Albert\x0a~~~\x0a- one\x0a- two\x0a- three\x0a~~~Einstein", "<p>Albert</p>\x0a<pre><code>- one\x0a- two\x0a- three\x0a</code></pre>\x0a<p>Einstein</p>\x0a"], # cm spec ignores Einstein
  [ "foo\x0a~~~\x0abar\x0a~~~\x0abaz","<p>foo</p>\x0a<pre><code>bar\x0a</code></pre>\x0a<p>baz</p>\x0a" ],

  [ "*	*	*	\x0a", "<p><em>	</em>	*	</p>\x0a" ],
  [ "***\x0a---\x0a___\x0a", "<hr>\x0a<hr>\x0a<hr>\x0a" ],
  [ " ***\x0a  ***\x0a   ***\x0a", "<p> <strong></strong>\x0a  ***\x0a   ***</p>\x0a" ],


  # Image hotlinks not supported
  [ "[![moon](moon.jpg)]", "<p>[![moon](moon.jpg)]</p>\x0a"],

  # Unicode 
  [ "Foo χρῆν\x0a","<p>Foo χρῆν</p>\x0a" ],
  [ "快点好","<p>快点好</p>\x0a"],
  [ "foo\\x0a","<p>foo\\x0a</p>\x0a" ],
  [ "foo  \x0a","<p>foo  </p>\x0a" ],
  [ "### foo\\x0a","<h3>foo\\x0a</h3>\x0a" ],
  [ "### foo  \x0a","<h3>foo  </h3>\x0a" ],
  [ "foo\x0abaz\x0a","<p>foo\x0abaz</p>\x0a" ],
  [ "foo \x0a baz\x0a","<p>foo \x0a baz</p>\x0a" ],
  [ "hello $.;'there\x0a","<p>hello $.;'there</p>\x0a" ],

  # Nesting
  [ "*a `*`*\x0a", "<p>*a <code>*</code>*</p>\x0a" ],
  [ "**a `**`*testing\x0a", "<p>**a <code>**</code>*testing</p>\x0a" ],
  [ "~~a `**~`~~testing\x0a", "<p>~~a <code>**~</code>~~testing</p>\x0a" ],
  [ "~~testing [test](http://alink.com)~~\x0a", "<p>~~testing <a href=\"http://alink.com\">test</a>~~</p>\x0a" ],
  [ "test `ing [test](http://alink.com)`\x0a", "<p>test <code>ing [test](http://alink.com)</code></p>\x0a" ],
  [ "**\***", "<p><strong>*</strong></p>\x0a" ],

  # Escaping
  [ "\*\* do not bold this **","<p>** do not bold this **</p>\x0a" ],
  [ "\* *this is bold\* *", "<p>*<em>this is bold* </em></p>\x0a" ],
  [ "\**hello*\*",          "<p>*<em>hello</em>*</p>\x0a" ],
  [ "*hello \* world*",     "<p><em>hello * world</em></p>\x0a"],
  [ "**hello \* world**",   "<p><strong>hello * world</strong></p>\x0a"],
  [ "No escape inside code blocks `\` ", "<p>No escape inside code blocks <code>\</code> </p>\x0a" ],
  #[ "&#X22; &#XD06; &#xcab;", "TODO" ],


#576 ><foo\+@bar.example.com>\x0a<
#576 ><p>&lt;foo+@bar.example.com&gt;</p>\x0a<


]

for t in tsts:
  eq( fm.render(t[0]), t[1], t[0] )



print("Testing Exceptions..")
raises( b"NaNd",         fm.render, TypeError, "The render argument must be a string" )

print("Testing garbage..")

# Make sure we don't seg fault on random unicode strings
for x in range(100):
  s = ""
  for z in range(random.randrange(0,2000)):
    s += chr(random.randrange(0,0x10FFFF))
  try:
    fm.render(s)
  except Exception as e:
    pass

# random ascii?
for x in range(100):
  s = ''.join(random.choice(string.ascii_letters) for m in range(random.randrange(1,20000)))
  fm.render(s)

print("Done")
