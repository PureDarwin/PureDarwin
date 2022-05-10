func SayHelloTo(name: String) -> String
{
  let greeting = "hello, " + name + "!"
  return greeting
}

func PrintHelloTo(name: String)
{
  let greeting = SayHelloTo(name:name)
  print(greeting)
}

PrintHelloTo(name:"world")
PrintHelloTo(name:"mike")
