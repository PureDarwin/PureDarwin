// NSDictionary literal support for PD's Foundation headers
@_exported import Foundation

extension NSDictionary : ExpressibleByDictionaryLiteral {
  public required convenience init(dictionaryLiteral elements: (Any, Any)...) {
    let keys = elements.map { $0.0 as AnyObject }
    let values = elements.map { $0.1 as AnyObject }
    self.init(objects: values as [AnyObject], forKeys: keys as [AnyObject])
  }
}
