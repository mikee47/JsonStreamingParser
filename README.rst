JsonStreamingParser
===================

.. highlight:: c++

A `Sming <https://github.com/SmingHub/Sming>`__ library for parsing
potentially huge json streams on devices with scarce memory.

This library is a port of `Squix78's parser <https://github.com/squix78/json-streaming-parser>`__,
itself a port of `Salsify's PHP based json streaming parser <https://github.com/salsify/jsonstreamingparser>`__.

This code is available under the MIT license, which basically means that you can use, modify the distribute
the code as long as you give credits to the authors and add a reference back to the original repository.
Please read the enclosed LICENSE for more detail.


Comparison to ArduinoJson
-------------------------

ArduinoJson is a DOM parser. That is, it scans a document and builds a set of objects which reflect
its content. Of course, it also allows documents to be created.

When reading larger JSON documents this requires too much memory. There are workarounds for this,
but the most efficient approach is to use a SAX (streaming) parser. This type of parser processes each element
and invokes a callback for each one. It is up to the application to decide what to do with the element
at each stage of parsing.


How to use
----------

Take a look in test/ReadTest.cpp for an example of how to user the parser. Here's the main function::

   BasicListener listener(output);
   JSON::StreamingParser<128> parser(&listener);
   auto status = parser.parse(input);
   debug_i("Parser returned '%s'", JSON::toString(status).c_str());
   return status == JSON::Status::EndOfDocument;

The parser requires a class inheriting from :cpp:class:`JSON::Listener`, implementing these two methods:

-  startElement
-  endElement

Note that this differs from Squix's implementation which required a separate method for each element type.

Each :cpp:struct:`Element` passed to these methods describes a single element, including its nesting level.

