// NSString literal and AnyHashable support for PD's Foundation headers
@_exported import Foundation

extension NSString : ExpressibleByStringLiteral {
  public required convenience init(stringLiteral value: StaticString) {
    // start from a CF string, PD's CFStringCreateCopy cannot read Swift string storage
    if value.hasPointerRepresentation {
      let p = UnsafeRawPointer(value.utf8Start).assumingMemoryBound(to: CChar.self)
      let s: NSString = NSString(utf8String: p)
      self.init(string: s as String)
      return
    }
    self.init(string: value.description)
  }
}

extension NSString : _HasCustomAnyHashableRepresentation {
  // @nonobjc avoids bridging AnyHashable back to NSObject
  @nonobjc
  public func _toCustomAnyHashable() -> AnyHashable? {
    return AnyHashable(self as String)
  }
}

extension NSString {
  public convenience init(format: __shared NSString, _ args: CVarArg...) {
    let va_args = getVaList(args)
    self.init(format: format as String, arguments: va_args)
  }
}
