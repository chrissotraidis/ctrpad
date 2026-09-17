#import <Foundation/Foundation.h>
/* Only explicitly supplied non-identifying metadata and bounded app logs. */
NSString *CTRPadDiagnosticsReport(NSDictionary<NSString *, id> *metadata);
NSString *CTRPadDiagnosticsRedact(NSString *text);
